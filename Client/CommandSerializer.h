#pragma once
#include "Constants.h"
#include "ByteMagic.h"
#include <cstddef>
#include <vector>
#include <stdexcept>
#include "Exceptions.h"

constexpr int MINIMUM_VOICE_FOCUS_STEP = 2;
constexpr unsigned int ASM_LEVEL_DISABLED = -1;

namespace CommandSerializer
{
	struct Message
	{
		DATA_TYPE dataType;
		unsigned char seqNumber;
		Buffer data;
	};

	//escape special chars

	Buffer _escapeSpecials(const Buffer& src);
	Buffer _unescapeSpecials(const Buffer& src);
	unsigned char _sumChecksum(const char* src, size_t size);
	unsigned char _sumChecksum(const Buffer& src);
	//Package a serialized command according to the protocol
	/*
	References:
	* DataType
	* CommandBluetoothSender.sendCommandWithRetries
	* BluetoothSenderWrapper.sendCommandViaBluetooth
	* 
	* Serialized data format: <START_MARKER>ESCAPE_SPECIALS(<DATA_TYPE><SEQ_NUMBER><BIG ENDIAN 4 BYTE SIZE OF UNESCAPED DATA><DATA><1 BYTE CHECKSUM>)<END_MARKER>
	*/
	Buffer packageDataForBt(const Buffer& src, DATA_TYPE dataType, unsigned int seqNumber);

	Message unpackBtMessage(const Buffer& src);

	NC_DUAL_SINGLE_VALUE getDualSingleForAsmLevel(char asmLevel);
	Buffer serializeNcAndAsmSetting(NC_ASM_EFFECT ncAsmEffect, NC_ASM_SETTING_TYPE ncAsmSettingType, ASM_SETTING_TYPE asmSettingType, ASM_ID asmId, char asmLevel);
	Buffer serializeVPTSetting(VPT_INQUIRED_TYPE type, unsigned char preset);

	//Inquiry commands and response parsers for reading current device state (battery, NC/ASM, VPT).
	//NOTE: These command bytes and payload layouts are based on public reverse-engineering of Sony's
	//MDR protocol (they aren't derived from official documentation), and haven't been verified against
	//real hardware. They may need adjustment once tested against an actual device.
	Buffer serializeBatteryInquiry(BATTERY_INQUIRED_TYPE type);
	Buffer serializeNcAndAsmInquiry();
	Buffer serializeVptInquiry();

	BatteryStatus parseBatteryLevel(const Buffer& payload);
	NcAsmStatus parseNcAndAsmSetting(const Buffer& payload);
	VptStatus parseVptSetting(const Buffer& payload);
}

