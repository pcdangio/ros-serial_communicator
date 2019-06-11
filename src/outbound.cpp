#include "serial_communicator/utility/outbound.h"

using namespace serial_communicator;
using namespace serial_communicator::utility;

outbound::outbound(message* message, unsigned int sequence_number, bool receipt_required, message_status* tracker)
{
    outbound::m_message = message;
    outbound::m_sequence_number = sequence_number;
    outbound::m_receipt_required = receipt_required;
    outbound::m_tracker = tracker;
}
