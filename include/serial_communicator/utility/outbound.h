/// \file outbound.h
/// \brief Defines the serial_communicator::utility::outbound class.
#ifndef OUTBOUND_H
#define OUTBOUND_H

#include "serial_communicator/message.h"
#include "serial_communicator/message_status.h"

namespace serial_communicator {
namespace utility {
///
/// \brief Provides management of outbound messages.
///
class outbound
{
public:
    // CONSTRUCTORS
    ///
    /// \brief outbound Initializes a new outbound instance.
    /// \param message The outbound message.
    /// \param sequence_number The originating sequence number of the outbound message.
    /// \param receipt_required A flag indicating if receipt is required for the outbound message.
    /// \param tracker A tracker for external observation of an outgoing message's status.
    ///
    outbound(message* message, unsigned int sequence_number, bool receipt_required, message_status* tracker);
    ~outbound();

private:
    ///
    /// \brief m_message Stores the outgoing message.
    ///
    message* m_message;
    ///
    /// \brief m_sequence_number Stores the originating sequence number of the outgoing message.
    ///
    unsigned int m_sequence_number;
    ///
    /// \brief m_receipt_required Stores the flag indicating if receipt is required for the outgoing message.
    ///
    bool m_receipt_required;
    ///
    /// \brief m_tracker Stores a pointer to the tracker for external observation of the outgoing message's status.
    ///
    message_status* m_tracker;
};
}}

#endif // OUTBOUND_H
