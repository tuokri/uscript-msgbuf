/*
 * Copyright (C) 2023  Tuomo Kriikkula
 * This program is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 *     along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

class UMBTestsTcpLink extends TcpLink;

`include(UMBTests\Classes\UMBTestsMacros.uci);

const PACKET_SIZE = 255;
const HEADER_SIZE = 4;
const MULTI_PART_END = 254;

var private int ClientPort;

var string TargetHost;
var int TargetPort;

var private int BytesSent;
var private int TotalBytesToSend;
var private int NumHeaderBytes;
var private byte NumPartsOut;
var private byte PartOut;
var private byte MTOut0;
var private byte MTOut1;
var private int Idx;
var private int BufIdx;
var private int NumSent;
var private int NumToSend;
var private byte OutBuf[PACKET_SIZE];

// Received message.
var byte Size;
var byte Part;
var int MessageType;
var bool bIsStatic;
var byte RecvMsgBufStatic[PACKET_SIZE];
var array<byte> RecvMsgBufMulti;

event Tick(float DeltaTime)
{
    super.Tick();

    if (!IsConnected())
    {
        return;
    }

    if (!IsDataPending())
    {
        return;
    }

    // Read size byte.
    // Read desired byte count.
    // Parse message. If multipart, go into state to wait for more bytes.
    // If single part, just wait for next size byte.
}

final function ConnectToServer()
{
    LinkMode = MODE_Binary;
    ReceiveMode = RMODE_Event;
    Resolve(TargetHost);
}

// TODO: dedicated function for sending static bytes.

// TODO: take Size, Part, MessageType as arguments here?
final function SendBytes(const out array<byte> Bytes)
{
    // TODO: should this be calculated somewhere else?
    NumHeaderBytes = FCeil(float(Bytes.Length - HEADER_SIZE) / PACKET_SIZE) * HEADER_SIZE;
    NumPartsOut = FCeil(float(NumHeaderBytes + (Bytes.Length - HEADER_SIZE)) / PACKET_SIZE);
    TotalBytesToSend = Bytes.Length + NumHeaderBytes - HEADER_SIZE;

    `ulog("Bytes.Length:" @ Bytes.Length
        @ "NumHeaderBytes:" @ NumHeaderBytes
        @ "NumPartsOut:" @ NumPartsOut
        @ "TotalBytesToSend:" @ TotalBytesToSend);

    if (NumPartsOut == 0)
    {
        `ulog("ERROR: invalid NumPartsOut:" @ NumPartsOut);
        `log("invalid NumPartsOut:" @ NumPartsOut,, 'ERROR');
        return;
    }

    // The easy case.
    // TODO: validate that more than PACKET_SIZE
    //   bytes are not trying to be sent in a single part?
    if (NumPartsOut == 1 || Bytes.Length <= PACKET_SIZE)
    {
        for (Idx = 0; Idx < Bytes.Length; ++Idx)
        {
            OutBuf[Idx] = Bytes[Idx];
        }

        OutBuf[1] = 255; // TODO: maybe document this a bit.

        NumToSend = Bytes.Length;
        NumSent = SendBinary(NumToSend, OutBuf);
        `ulog("Sent" @ NumSent @ "bytes");
        return;
    }

    // Multipart messages.
    // Skip first header.
    Idx = 4;
    BytesSent = 4;

    MTOut0 = Bytes[2];
    MTOut1 = Bytes[3];

    for (PartOut = 0; PartOut < NumPartsOut; ++PartOut)
    {
        if (PartOut == (NumPartsOut - 1))
        {
            PartOut = MULTI_PART_END;
        }

        BufIdx = 0;
        NumToSend = Min(PACKET_SIZE, TotalBytesToSend - BytesSent);
        OutBuf[BufIdx++] = NumToSend;
        OutBuf[BufIdx++] = PartOut;
        OutBuf[BufIdx++] = MTOut0;
        OutBuf[BufIdx++] = MTOut1;

        // TODO: does this error with the first header?
        while (BufIdx < NumToSend)
        {
            OutBuf[BufIdx++] = Bytes[Idx++];
        }

        NumSent = SendBinary(NumToSend, OutBuf);
        BytesSent += NumToSend;
        `ulog("Sent" @ NumSent @ "bytes," @ "part:" @ PartOut @ "NumToSend:" @ NumToSend @ "BytesSent:" @ BytesSent);
    }
}

event Resolved(IpAddr Addr)
{
    ClientPort = BindPort();
    if (ClientPort == 0)
    {
        `ulog("##ERROR##: failed to bind port");
        return;
    }

    Addr.Port = TargetPort;
    if (!Open(Addr))
    {
        `ulog("##ERROR##: open failed");
    }
}

event ResolveFailed()
{
    `ulog("##ERROR##: resolve failed");
    ConnectToServer();
}

event Opened()
{
    `ulog("opened");
}

event Closed()
{
    `ulog("closed");
}

// TODO: this gives garbage if packet size >= 255.
event ReceivedBinary(int Count, byte B[PACKET_SIZE])
{
    local int I;
    local int J;

    `ulog("Count       :" @ Count);

    Size = B[0];
    Part = B[1];
    MessageType = B[2] | (B[3] << 8);

    `ulog("Size        :" @ Size);
    `ulog("Part        :" @ Part);
    `ulog("MessageType :" @ MessageType);

    bIsStatic = class'TestMessages'.static.IsStaticMessage(MessageType);
    if (bIsStatic)
    {
        // The easy case.
        if (Size <= PACKET_SIZE)
        {
            for (J = 0; J < Size; ++J)
            {
                RecvMsgBufStatic[J] = B[J];
            }
        }
        // TODO: this check makes no fucking sense.
        else
        {
            `ulog("##ERROR##: message too large to be static!");
            `log("##ERROR##: message too large to be static!",, 'ERROR');
        }

        return;
    }

    I = 4;
}

DefaultProperties
{
    TargetHost="127.0.0.1"
    TargetPort=55555

    TickGroup=TG_DuringAsyncWork
}
