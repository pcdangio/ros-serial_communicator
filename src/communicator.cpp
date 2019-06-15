#include "serial_communicator/communicator.h"

using namespace serial_communicator;

// CONSTRUCTORS
communicator::communicator(std::string port, unsigned int baud, unsigned int data_bits, unsigned int parity_bits, unsigned int stop_bits)
{
    // Set up the serial port.
    communicator::m_serial_port = new serial::Serial(port, baud, serial::Timeout::simpleTimeout(30),
                                                     static_cast<serial::bytesize_t>(data_bits),
                                                     static_cast<serial::parity_t>(parity_bits),
                                                     static_cast<serial::stopbits_t>(stop_bits),
                                                     serial::flowcontrol_t::flowcontrol_none);
    communicator::m_serial_port->flush();

    // Initialize parameters to default values.
    communicator::m_queue_size = 10;
    communicator::m_receipt_timeout = 100;
    communicator::m_max_transmissions = 5;

    // Initialize sequence counter.
    communicator::m_sequence_counter = 0;

    // Initialize queues.
    communicator::m_tx_queue = new utility::outbound*[communicator::m_queue_size];
    communicator::m_rx_queue = new utility::inbound*[communicator::m_queue_size];
    for(unsigned short i = 0; i < communicator::m_queue_size; i++)
    {
        communicator::m_tx_queue[i] = nullptr;
        communicator::m_rx_queue[i] = nullptr;
    }
}
communicator::~communicator()
{
    // Clean up queues.
    for(unsigned short i = 0; i < communicator::m_queue_size; i++)
    {
        if(communicator::m_tx_queue[i] != nullptr)
        {
            delete communicator::m_tx_queue[i];
        }
        if(communicator::m_rx_queue[i] != nullptr)
        {
            delete communicator::m_rx_queue[i];
        }
    }
    delete [] communicator::m_tx_queue;
    delete [] communicator::m_rx_queue;

    // Clean up the serial port.
    communicator::m_serial_port->close();
    delete communicator::m_serial_port;
}

// PUBLIC METHODS
bool communicator::send(message* message, bool receipt_required, message_status* tracker)
{
    // Find an open spot in the transmit queue.
    for(unsigned short i = 0; i < communicator::m_queue_size; i++)
    {
        if(communicator::m_tx_queue[i] == nullptr)
        {
            // Open space found. Add outbound message and increment sequence counter.
            communicator::m_tx_queue[i] = new utility::outbound(message, communicator::m_sequence_counter++, receipt_required, tracker);
            // Quit here.
            return true;
        }
    }

    // If this point reached, a spot was not found.
    delete message;
    return false;
}
unsigned short communicator::messages_available() const
{
    // Count and return total number of messages in receive queue.
    unsigned short n_messages = 0;
    for(unsigned short i = 0; i < communicator::m_queue_size; i++)
    {
        if(communicator::m_rx_queue[i] != nullptr)
        {
            n_messages++;
        }
    }
    return n_messages;
}
message* communicator::receive(unsigned short id)
{
    // Find a message with the matching ID that has the highest priority, followed by oldest age.
    utility::inbound* to_read = nullptr;
    unsigned short location = 0;

    for(unsigned short i = 0; i < communicator::m_queue_size; i++)
    {
        // Check if there is a valid message at this location.
        if(communicator::m_rx_queue[i] != nullptr)
        {
            // Store local reference to this message.
            utility::inbound* current = communicator::m_rx_queue[i];

            // Check if the message has a matching id.
            if(id == 0xFFFF || current->p_message()->p_id() == id)
            {
                // If to_read is currently empty, initialize it.
                if(to_read == nullptr)
                {
                    to_read = current;
                    location = i;
                }
                else
                {
                    // Check to see if the current message beats the to_read message in priority.
                    if(current->p_message()->p_priority() > to_read->p_message()->p_priority())
                    {
                        // Replace the to_read message with the current message.
                        to_read = current;
                        location = i;
                    }
                    // Otherwise, check if priorities are equal.
                    else if(current->p_message()->p_priority() == to_read->p_message()->p_priority())
                    {
                        // Compare age.
                        if(current->p_sequence_number() < to_read->p_sequence_number())
                        {
                            // Replace to_read message with current message.
                            to_read = current;
                            location = i;
                        }
                    }
                }

            }
        }
    }

    // Check if a message was actually found.
    if(to_read == nullptr)
    {
        return nullptr;
    }

    // If this point has been reached, a valid message has been found to read.

    // Extract the message from the inbound instance before it is deleted.
    message* output = to_read->p_message();

    // Remove the inbound entry from the receive queue.
    delete communicator::m_rx_queue[location];
    communicator::m_rx_queue[location] = nullptr;

    // Return the read message.
    return output;
}
void communicator::spin()
{
    // First send messages.
    communicator::spin_tx();

    // Next, receive messages.
    communicator::spin_rx();
}

// PUBLIC PROPERTIES
unsigned short communicator::p_queue_size()
{
    return communicator::m_queue_size;
}
void communicator::p_queue_size(unsigned short value)
{
    // Check if a resize is necessary.
    if(value != communicator::m_queue_size)
    {
        // Resize the queues.
        // Create new queues.
        utility::outbound** new_tx = new utility::outbound*[value];
        utility::inbound** new_rx = new utility::inbound*[value];

        // Copy current queues into new queues.
        for(unsigned short i = 0; i < communicator::m_queue_size; i++)
        {
            new_tx[i] = communicator::m_tx_queue[i];
            new_rx[i] = communicator::m_rx_queue[i];
        }

        // Fill any remaining space with nullptrs.
        for(unsigned short i = communicator::m_queue_size; i < value; i++)
        {
            new_tx[i] = nullptr;
            new_rx[i] = nullptr;
        }

        // Delete old queues and replace them.
        delete [] communicator::m_tx_queue;
        delete [] communicator::m_rx_queue;
        communicator::m_tx_queue = new_tx;
        communicator::m_rx_queue = new_rx;

        // Update the queue size variable.
        communicator::m_queue_size = value;
    }
}
unsigned int communicator::p_receipt_timeout()
{
    return communicator::m_receipt_timeout;
}
void communicator::p_receipt_timeout(unsigned int value)
{
    communicator::m_receipt_timeout = value;
}
unsigned char communicator::p_max_transmissions()
{
    return communicator::m_max_transmissions;
}
void communicator::p_max_transmissions(unsigned char value)
{
    communicator::m_max_transmissions = value;
}

// PRIVATE METHODS
void communicator::spin_tx()
{
    // Send the message with the highest priority or age.

    // First, find the message with the highest priority or age.
    utility::outbound* to_send = nullptr;
    unsigned short location = 0;
    for(unsigned short i = 0; i < communicator::m_queue_size; i++)
    {
        // Check if this address has a valid outbound message in it.
        if(communicator::m_tx_queue[i] != nullptr)
        {
            // Get local reference to the current message.
            utility::outbound* current = communicator::m_tx_queue[i];

            // Check if this outbound is actively awaiting for receipt (e.g. hasn't yet timed out)
            if(current->p_status() == message_status::VERIFYING && current->timeout_elapsed(communicator::m_receipt_timeout) == false)
            {
                // Message timeout hasn't expired for receipt yet, so no need to resend at this time.
                // Skip this message.
                continue;
            }

            // Check if to_send has a value yet.
            if(to_send == nullptr)
            {
                // Update to_send with current value.
                to_send = current;
                location = i;
            }
            else
            {
                // Check if current message has greater priority.
                if(current->p_message()->p_priority() > to_send->p_message()->p_priority())
                {
                    // Update to_send with current value.
                    to_send = current;
                    location = i;
                }
                // Check if current message has equal priority.
                else if(current->p_message()->p_priority() == to_send->p_message()->p_priority())
                {
                    // Check if current message has older age.
                    if(current->p_sequence_number() < to_send->p_sequence_number())
                    {
                        // Update to_send with current value.
                        to_send = current;
                        location = i;
                    }
                }
            }
        }
    }

    // Check that a message was actually found to send.
    if(to_send == nullptr)
    {
        return;
    }

    // At this point, to_send contains the appropriate message to send.
    // Check if this is the first time the message is being sent.
    if(to_send->p_n_transmissions() == 0)
    {
        // Message has not been sent yet.
        // Send the message.
        communicator::tx(to_send);
        // Check if receipt is required.
        if(to_send->p_receipt_required())
        {
            // Receipt is required.
            // Leave in the tx queue and update status.
            to_send->update_status(message_status::VERIFYING);
        }
        else
        {
            // Receipt is not required.
            // Update status to sent and delete from queue.
            to_send->update_status(message_status::SENT);
            delete communicator::m_tx_queue[location];
            communicator::m_tx_queue[location] = nullptr;
        }
    }
    else
    {
        // Message has been sent at least once and has timed out waiting for a receipt.
        // Check if message can be resent.
        if(to_send->can_retransmit(communicator::m_max_transmissions))
        {
            // Message can be resent.
            communicator::tx(to_send);
        }
        else
        {
            // Message has already been sent the maximum number of times.
            // Update status and delete.
            to_send->update_status(message_status::NOTRECEIVED);
            delete communicator::m_tx_queue[location];
            communicator::m_tx_queue[location] = nullptr;
        }
    }
}
void communicator::spin_rx()
{
    // Read bytes until header byte is found or timed out.
    // Do this directly from the serial port since the header is not concerned with escape bytes.
    unsigned char read_byte = 0;
    while(read_byte != communicator::m_header_byte)
    {
        unsigned long n_read = communicator::m_serial_port->read(&read_byte, 1);
        if(n_read < 1)
        {
            // Timeout has occured, quit.
            return;
        }
    }

    // If this point reached, a valid header has been found.
    // Message data length is needed.  Read beginning of packet into temporary array to get to length.
    // 1 header, 4 sequence, 1 receipt, 2 message id, 1 priority, 2 data length. Read next 10 bytes.
    unsigned char packet_front[11];
    packet_front[0] = read_byte;
    // Use rx method for rest of reads to handle escape bytes.
    if(communicator::rx(&packet_front[1], 10) == false)
    {
        // Timeout has occurred, quit.
        return;
    }

    // Extract the data length from the end of packet_front.
    unsigned short data_length = be16toh(*reinterpret_cast<unsigned short*>(&packet_front[9]));

    // Form the final packet array.
    unsigned int packet_length = 11 + data_length + 1;
    unsigned char* packet = new unsigned char[packet_length];
    // Copy the front of the packet into the final packet.
    std::memcpy(packet, packet_front, 11);
    // Read the remaining bytes into the packet.
    if(communicator::rx(&packet[11], data_length + 1) == false)
    {
        // Timeout has occurred, quit.
        return;
    }

    // If this point is reached, a full packet has been read.

    // Validate the checksum.
    bool checksum_ok = packet[packet_length-1] == communicator::checksum(packet, packet_length-1);
    // Extract sequence number from the packet.
    unsigned int sequence_number = be32toh(*reinterpret_cast<unsigned int*>(&packet[1]));

    // Handle receipts
    switch(static_cast<communicator::receipt_type>(packet[5]))
    {
    case communicator::receipt_type::NOT_REQUIRED:
    {
        // Do nothing
        break;
    }
    case communicator::receipt_type::REQUIRED:
    {
        // Draft and send a receipt message outside of the typical outbound/tx_queue.
        // Receipt messages do not need to be tracked.
        unsigned char receipt[12];
        // Copy header, sequence, receipt, id, and priority back into receipt.
        std::memcpy(receipt, packet, 9);
        // Update the receipt field.
        if(checksum_ok)
        {
            receipt[5] = static_cast<unsigned char>(communicator::receipt_type::RECEIVED);
        }
        else
        {
            receipt[5] = static_cast<unsigned char>(communicator::receipt_type::CHECKSUM_MISMATCH);
        }
        // No data fields.
        receipt[9] = 0;
        receipt[10] = 0;
        // Set checksum.
        receipt[11] = communicator::checksum(receipt, 11);
        // Write message.
        communicator::tx(receipt, 12);
        break;
    }
    case communicator::receipt_type::RECEIVED:
    {
        // If checksum is ok, remove the associated message from the TXQ if it is still in there.
        if(checksum_ok)
        {
            for(unsigned short i = 0; i < communicator::m_queue_size; i++)
            {
                if(communicator::m_tx_queue[i] != nullptr)
                {
                    utility::outbound* current = communicator::m_tx_queue[i];
                    if(current->p_sequence_number() == sequence_number)
                    {
                        // Update the message's status.
                        current->update_status(message_status::RECEIVED);
                        // Remove it from the queue.
                        delete communicator::m_tx_queue[i];
                        communicator::m_tx_queue[i] = nullptr;
                        // Quit the for loop.
                        break;
                    }
                }
            }
        }
        break;
    }
    case communicator::receipt_type::CHECKSUM_MISMATCH:
    {
        // Find the associated message based on sequence number and immediately resend it.
        if(checksum_ok)
        {
            for(unsigned short i = 0; i < communicator::m_queue_size; i++)
            {
                if(communicator::m_tx_queue[i] != nullptr)
                {
                    utility::outbound* current = communicator::m_tx_queue[i];
                    if(current->p_sequence_number() == sequence_number)
                    {
                        // Check if message can be resent.
                        if(current->can_retransmit(communicator::m_max_transmissions))
                        {
                            // Message can be resent.
                            communicator::tx(current);
                        }
                        else
                        {
                            // Message has already been sent the maximum number of times.
                            // Update status and delete.
                            current->update_status(message_status::NOTRECEIVED);
                            delete communicator::m_tx_queue[i];
                            communicator::m_tx_queue[i] = nullptr;
                        }
                        // Quit the for loop.
                        break;
                    }
                }
            }
        }
        break;
    }
    }

    // Lastly, put packet into inbound message in the rx_queue.
    if(checksum_ok)
    {
        // Find an open position in the RXQ.
        for(unsigned short i = 0; i < communicator::m_queue_size; i++)
        {
            if(communicator::m_rx_queue[i] == nullptr)
            {
                // Extract the message from the packet.
                message* msg = new message(&packet[6]);
                // Add new inbound to the rx_queue.
                communicator::m_rx_queue[i] = new utility::inbound(msg, sequence_number);

                // Exit for loop.
                break;
            }
        }
    }

    // Delete the packet.
    delete [] packet;
}
void communicator::tx(utility::outbound* message)
{
    // Serialize the packet without escapes.
    // First, get total packet length = message length + 7 (1 header, 4 sequence, 1 receipt, 1 checksum)
    unsigned int packet_size = message->p_message()->p_message_length() + 7;
    // Create packet.
    unsigned char* packet = new unsigned char[packet_size];
    // Write the header, sequence, and receipt.
    packet[0] = communicator::m_header_byte;
    unsigned int be_sequence = htobe32(message->p_sequence_number());
    std::memcpy(&packet[1], &be_sequence, 4);
    packet[5] = message->p_receipt_required();
    // Write the message bytes.
    message->p_message()->serialize(&packet[6]);
    // Calculate and add CRC.
    packet[packet_size-1] = communicator::checksum(packet, packet_size - 1);

    // Write to the serial port.
    communicator::tx(packet, packet_size);

    // Mark that the message has been sent.
    message->mark_transmitted();

    // Delete the packet.
    delete [] packet;
}
void communicator::tx(unsigned char *buffer, unsigned int length)
{
    // Check if escapes are needed.
    unsigned int n_escapes = 0;
    // Only check after the header.
    for(unsigned int i = 1; i < length; i++)
    {
        if(buffer[i] == communicator::m_header_byte || buffer[i] == communicator::m_escape_byte)
        {
            n_escapes++;
        }
    }

    // Handle escapes.
    if(n_escapes > 0)
    {
        // Escapes needed.
        unsigned char* esc_buffer = new unsigned char[length + n_escapes];
        unsigned int esc_write_position = 0;
        // Copy the header byte first since it should not be escaped.
        esc_buffer[esc_write_position++] = buffer[0];
        // Only check after the header.
        for(unsigned int i = 1; i < length; i++)
        {
            if(buffer[i] == communicator::m_header_byte || buffer[i] == communicator::m_escape_byte)
            {
                // Insert escape.
                esc_buffer[esc_write_position++] = communicator::m_escape_byte;
                // Copy in byte decremented by one.
                esc_buffer[esc_write_position++] = buffer[i] - 1;
            }
            else
            {
                // Copy buffer byte in.
                esc_buffer[esc_write_position++] = buffer[i];
            }
        }

        // Write the escaped buffer.
        communicator::m_serial_port->write(esc_buffer, length + n_escapes);
        // Delete the escaped buffer.
        delete [] esc_buffer;
    }
    else
    {
        // Escapes not needed.  Write buffer as is.
        communicator::m_serial_port->write(buffer, length);
    }
}
bool communicator::rx(unsigned char* buffer, unsigned int length)
{
    // Create global flag for unescaping the next byte, even across different read segments.
    bool unescape_next = false;
    // Block read until length bytes have been satisfied after escapements.
    unsigned int current_length = 0;
    while(current_length < length)
    {
        // Read in the remaining length into a temporary buffer.
        unsigned int remaining_length = length - current_length;
        unsigned char* temp_buffer = new unsigned char[remaining_length];
        unsigned long n_read = communicator::m_serial_port->read(temp_buffer, remaining_length);
        if(n_read < remaining_length)
        {
            // Quit due to timeout.
            return false;
        }
        // Read through the temporary buffer and extract bytes into the actual buffer.
        for(unsigned int i = 0; i < remaining_length; i++)
        {
            if(temp_buffer[i] == communicator::m_escape_byte)
            {
                // Mark the escape flag.
                unescape_next = true;
            }
            else
            {
                // Copy byte.
                // Unescaping is adding 1 to the value. Can use cast of unescape_next bool.
                buffer[current_length++] = temp_buffer[i] + static_cast<unsigned char>(unescape_next);
                // Set unescape flag.
                unescape_next = false;
            }
        }

        delete [] temp_buffer;
    }

    // If this point is reached, current_length = length.
    return true;
}
unsigned char communicator::checksum(unsigned char* data, unsigned int length)
{
    unsigned char checksum = 0;
    for(unsigned int i = 0 ; i < length; i++)
    {
        checksum ^= data[i];
    }
    return checksum;
}
