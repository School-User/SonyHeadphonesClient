#include "BluetoothWrapper.h"

//Bounds how many complete messages _receiveMessage() will read while looking for a specific
//message (an ACK, or a given inquiry's response), skipping over unrelated notifications.
constexpr int MAX_MESSAGES_TO_SKIP = 5;

BluetoothWrapper::BluetoothWrapper(std::unique_ptr<IBluetoothConnector> connector)
{
	this->_connector.swap(connector);
}

BluetoothWrapper::BluetoothWrapper(BluetoothWrapper&& other) noexcept
{
	this->_connector.swap(other._connector);
	this->_seqNumber = other._seqNumber;
}

BluetoothWrapper& BluetoothWrapper::operator=(BluetoothWrapper&& other) noexcept
{
	//self assignment
	if (this == &other) return *this;

	this->_connector.swap(other._connector);
	this->_seqNumber = other._seqNumber;

	return *this;
}

int BluetoothWrapper::sendCommand(const std::vector<char>& bytes)
{
	std::lock_guard guard(this->_connectorMtx);
	auto data = CommandSerializer::packageDataForBt(bytes, DATA_TYPE::DATA_MDR, this->_seqNumber++);
	auto bytesSent = this->_connector->send(data.data(), data.size());

	//An unrelated notification (e.g. a battery or settings NTFY_PARAM the device sends on its own)
	//can arrive before the ACK for this command, so skip past anything that isn't the ACK itself.
	for (int i = 0; i < MAX_MESSAGES_TO_SKIP; i++)
	{
		if (this->_receiveMessage().dataType == DATA_TYPE::ACK)
		{
			return bytesSent;
		}
	}

	throw RecoverableException("Didn't receive an acknowledgement for the command", false);
}

Buffer BluetoothWrapper::sendCommandAndGetResponse(const Buffer& bytes, COMMAND_TYPE expectedResponseType)
{
	std::lock_guard guard(this->_connectorMtx);
	auto data = CommandSerializer::packageDataForBt(bytes, DATA_TYPE::DATA_MDR, this->_seqNumber++);
	this->_connector->send(data.data(), data.size());

	for (int i = 0; i < MAX_MESSAGES_TO_SKIP; i++)
	{
		auto msg = this->_receiveMessage();
		if (!msg.data.empty() && static_cast<COMMAND_TYPE>(msg.data[0]) == expectedResponseType)
		{
			return msg.data;
		}
	}

	throw RecoverableException("Timed out waiting for a response to an inquiry", false);
}

bool BluetoothWrapper::isConnected() noexcept
{
	return this->_connector->isConnected();
}

void BluetoothWrapper::connect(const std::string& addr)
{
	std::lock_guard guard(this->_connectorMtx);
	this->_connector->connect(addr);
}

void BluetoothWrapper::disconnect() noexcept
{
	std::lock_guard guard(this->_connectorMtx);
	this->_seqNumber = 0;
	this->_connector->disconnect();
}


std::vector<BluetoothDevice> BluetoothWrapper::getConnectedDevices()
{
	return this->_connector->getConnectedDevices();
}

CommandSerializer::Message BluetoothWrapper::_receiveMessage()
{
	bool ongoingMessage = false;
	bool messageFinished = false;
	char buf[MAX_BLUETOOTH_MESSAGE_SIZE] = { 0 };
	Buffer msgBytes;

	do
	{
		auto numRecvd = this->_connector->recv(buf, sizeof(buf));
		size_t messageStart = 0;
		size_t messageEnd = numRecvd;

		for (size_t i = 0; i < numRecvd; i++)
		{
			if (buf[i] == START_MARKER)
			{
				if (ongoingMessage)
				{
					throw RecoverableException("Invalid: Multiple start markers without an end marker", true);
				}
				messageStart = i + 1;
				ongoingMessage = true;
			}
			else if (ongoingMessage && buf[i] == END_MARKER)
			{
				messageEnd = i;
				ongoingMessage = false;
				messageFinished = true;
			}
		}

		msgBytes.insert(msgBytes.end(), buf + messageStart, buf + messageEnd);
	} while (!messageFinished);

	auto msg = CommandSerializer::unpackBtMessage(msgBytes);
	this->_seqNumber = msg.seqNumber;
	return msg;
}

