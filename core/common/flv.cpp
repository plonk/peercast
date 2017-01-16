// ------------------------------------------------
// File : flv.cpp
// Date: 14-jan-2017
// Author: niwakazoider
//
// (c) 2002-3 peercast.org
// ------------------------------------------------
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// ------------------------------------------------

#include "channel.h"
#include "flv.h"
#include "string.h"
#include "stdio.h"
#ifdef _DEBUG
#include "chkMemoryLeak.h"
#define DEBUG_NEW new(__FILE__, __LINE__)
#define new DEBUG_NEW
#endif


// ------------------------------------------
void FLVStream::readEnd(Stream &, Channel *)
{
}

// ------------------------------------------
void FLVStream::readHeader(Stream &, Channel *)
{
}

// ------------------------------------------
int FLVStream::readPacket(Stream &in, Channel *ch)
{
	bool headerUpdate = false;

	if (ch->streamPos == 0) {
		if (!fileHeader.read(in)) return 1;
		headerUpdate = true;
	}

	FLVTag flvTag;
	if (!flvTag.read(in)) return 1;
	
	switch (flvTag.type)
	{
		case TAG_SCRIPTDATA:
		{
			AMFObject amf;
			if (amf.readMetaData(flvTag)) {
				metaData = flvTag;
				bitrate = amf.bitrate;
				headerUpdate = true;
			}
		}
		case TAG_VIDEO:
		{
			//AVC Header
			if (flvTag.data[0] == 0x17 && flvTag.data[1] == 0x00 &&
				flvTag.data[2] == 0x00 && flvTag.data[3] == 0x00) {
				avcHeader = flvTag;
				headerUpdate = true;
			}
		}
		case TAG_AUDIO:
		{
			//AAC Header
			if (flvTag.data[0] == 0xaf && flvTag.data[1] == 0x00) {
				aacHeader = flvTag;
				headerUpdate = true;
			}
		}
	}
	
	if (headerUpdate) {
		int len = fileHeader.size;
		int offset = 0;

		if (metaData.type==TAG_SCRIPTDATA) len += metaData.packetSize;
		if (avcHeader.type == TAG_VIDEO)   len += avcHeader.packetSize;
		if (aacHeader.type == TAG_AUDIO)   len += aacHeader.packetSize;

		ch->headPack.init(ChanPacket::T_HEAD, ch->headPack.data, len, ch->streamPos);

		if (fileHeader.size > 0) {
			memcpy(ch->headPack.data + offset, fileHeader.data, fileHeader.size);
			offset += fileHeader.size;
		}
		if (metaData.type == TAG_SCRIPTDATA) {
			memcpy(ch->headPack.data + offset, metaData.packet, metaData.packetSize);
			offset += metaData.packetSize;
		}
		if (avcHeader.type == TAG_VIDEO) {
			memcpy(ch->headPack.data + offset, avcHeader.packet, avcHeader.packetSize);
			offset += avcHeader.packetSize;
		}
		if (aacHeader.type == TAG_AUDIO) {
			memcpy(ch->headPack.data + offset, aacHeader.packet, aacHeader.packetSize);
			offset += aacHeader.packetSize;
		}

		ch->info.bitrate = bitrate;

		ch->headPack.type = ChanPacket::T_HEAD;
		ch->headPack.len = len;
		ch->headPack.pos = ch->streamPos;
		ch->newPacket(ch->headPack);

		ch->streamPos += ch->headPack.len;
	} else {
		ChanPacket pack;

		int rlen = flvTag.packetSize;
		while (rlen)
		{
			int rl = rlen;
			if (rl > ChanMgr::MAX_METAINT)
				rl = ChanMgr::MAX_METAINT;

			pack.init(ChanPacket::T_DATA, pack.data, rl, ch->streamPos);

			int offset = flvTag.packetSize - rlen;
			memcpy(pack.data, flvTag.packet + offset, rl);

			ch->newPacket(pack);
			ch->checkReadDelay(pack.len);
			ch->streamPos += pack.len;

			rlen -= rl;
		}

		flvTag.dispose();
	}
	
	return 0;
}
