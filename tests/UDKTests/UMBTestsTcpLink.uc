/*
 * Copyright (C) 2023-2024  Tuomo Kriikkula
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
const SINGLE_PART = 255;
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
var private int ByteIdx;
var private int OutBufIdx;
var private int NumSent;
var private int NumToSend;
var private byte OutBuf[PACKET_SIZE];

// Received message.
var private int In_RecvBufIdx;
var private int In_MsgBufIdx;
var private int In_ReadCount;
var byte In_NumToRead;
var byte In_Size;
var byte In_Part;
var int In_MessageType;
var bool In_bIsStatic;

var int NumMessagesReceived;

// TODO: Should have event based system for indicating when a message of
//       certain type has been received. Currently relying on the user
//       manually checking the link's internal variable states.

enum EReadState
{
    ERS_WaitingForHeader,        // Default state.
    ERS_WaitingForMultiHeader,   // Received multipart payload earlier, waiting for next header.
    ERS_WaitingForSingleBytes,   // Header parsed, expecting single-part payload.
    ERS_WaitingForMultiBytes,    // Header parsed, expecting multipart payload.
};
var EReadState ReadState;

// Reusable buffer for receiving bytes.
var byte RecvMsgBuf[PACKET_SIZE];

// Final message bytes that can be fed to UMB
// X_FromBytes and X_FromMultiBytes functions.
var byte RecvMsgBufSingle[PACKET_SIZE];
var array<byte> RecvMsgBufMulti;

// TODO: Should messages be classes instead of structs?
// - Provide a way to generate both, or choose one or the other?
// TODO: Should really benchmark it.
// Passing around object references. Using new operator when creating messages.
// -> OO way, makes life a bit easier in certain parts.
// -> Does it lead to more copying of data?

event Tick(float DeltaTime)
{
    super.Tick(DeltaTime);

    if (!IsConnected())
    {
        return;
    }

    if (!IsDataPending())
    {
        return;
    }

    // TODO: limit the time we can spend here in a single tick?
    //       E.g., max iteration count?
    while (True)
    {
        `ulog("ReadState      :" @ ReadState);

        switch (ReadState)
        {
        case ERS_WaitingForHeader:

            In_ReadCount = ReadBinary(HEADER_SIZE, RecvMsgBuf);
            `ulog("In_ReadCount   :" @ In_ReadCount);

            // TCP buffer empty, try again next tick.
            if (In_ReadCount == 0)
            {
                return;
            }

            if (In_ReadCount < HEADER_SIZE)
            {
                `uerror("Expected" @ HEADER_SIZE @ "header bytes, got" @ In_ReadCount);
                break;
            }

            In_Size = RecvMsgBuf[0];
            In_Part = RecvMsgBuf[1];
            In_MessageType = RecvMsgBuf[2] | (RecvMsgBuf[3] << 8);

            `ulog("In_Size        :" @ In_Size);
            `ulog("In_Part        :" @ In_Part);
            `ulog("In_MessageType :" @ In_MessageType);

            // TODO: this check is unnecessary. Does the test mutator code using
            //       this actually need it for anything?
            In_bIsStatic = class'TestMessages'.static.IsStaticMessage(In_MessageType);

            if (In_Part == SINGLE_PART)
            {
                RecvMsgBufSingle[0] = RecvMsgBuf[0];
                RecvMsgBufSingle[1] = RecvMsgBuf[1];
                RecvMsgBufSingle[2] = RecvMsgBuf[2];
                RecvMsgBufSingle[3] = RecvMsgBuf[3];
                In_MsgBufIdx = HEADER_SIZE;
                ReadState = ERS_WaitingForSingleBytes;
            }
            else if (In_Part < MULTI_PART_END)
            {
                // Pre-allocate at least In_Size bytes. Final size not known yet.
                RecvMsgBufMulti.Length = In_Size;
                RecvMsgBufMulti[0] = RecvMsgBuf[0];
                RecvMsgBufMulti[1] = RecvMsgBuf[1];
                RecvMsgBufMulti[2] = RecvMsgBuf[2];
                RecvMsgBufMulti[3] = RecvMsgBuf[3];
                In_MsgBufIdx = HEADER_SIZE;
                ReadState = ERS_WaitingForMultiBytes;
            }
            else
            {
                `uerror("unexpected part:" @ In_Part @ "in state:" @ ReadState);
                return;
            }

            break;

        case ERS_WaitingForMultiHeader:

            In_ReadCount = ReadBinary(HEADER_SIZE, RecvMsgBuf);
            `ulog("In_ReadCount   :" @ In_ReadCount);

            // TCP buffer empty, try again next tick.
            if (In_ReadCount == 0)
            {
                return;
            }

            if (In_ReadCount < HEADER_SIZE)
            {
                `uerror("Expected" @ HEADER_SIZE @ "header bytes, got" @ In_ReadCount);
                break;
            }

            In_Size = RecvMsgBuf[0];
            In_Part = RecvMsgBuf[1];
            In_MessageType = RecvMsgBuf[2] | (RecvMsgBuf[3] << 8);

            `ulog("In_Size        :" @ In_Size);
            `ulog("In_Part        :" @ In_Part);
            `ulog("In_MessageType :" @ In_MessageType);

            if (In_Part <= MULTI_PART_END)
            {
                RecvMsgBufMulti.Length = RecvMsgBufMulti.Length + (In_Size - HEADER_SIZE);
                ReadState = ERS_WaitingForMultiBytes;
            }
            else
            {
                `uerror("unexpected part:" @ In_Part @ "in state:" @ ReadState);
                ReadState = ERS_WaitingForHeader;
            }

            break;

        case ERS_WaitingForSingleBytes:

            // TODO: what if there's not enough bytes read yet?
            //       Is that possible? Seems unlikely but need to
            //       check at some point whether it can actually happen.
            In_NumToRead = In_Size - HEADER_SIZE;
            In_ReadCount = ReadBinary(In_NumToRead, RecvMsgBuf);
            `ulog("In_ReadCount   :" @ In_ReadCount);

            // TCP buffer empty, try again next tick.
            if (In_ReadCount == 0)
            {
                return;
            }

            if (In_ReadCount != In_NumToRead)
            {
                `uerror("Expected" @ In_NumToRead @ "bytes, got" @ In_ReadCount);
                ReadState = ERS_WaitingForHeader;
                break;
            }

            if (In_bIsStatic)
            {
                for (In_RecvBufIdx = 0; In_RecvBufIdx < In_ReadCount; ++In_RecvBufIdx)
                {
                    RecvMsgBufSingle[In_MsgBufIdx++] = RecvMsgBuf[In_RecvBufIdx];
                }
            }
            else
            {
                for (In_RecvBufIdx = 0; In_RecvBufIdx < In_ReadCount; ++In_RecvBufIdx)
                {
                    RecvMsgBufMulti[In_MsgBufIdx++] = RecvMsgBuf[In_RecvBufIdx];
                }
            }

            ++NumMessagesReceived;
            ReadState = ERS_WaitingForHeader;

            break;

        case ERS_WaitingForMultiBytes:

            In_NumToRead = In_Size - HEADER_SIZE;
            In_ReadCount = ReadBinary(In_NumToRead, RecvMsgBuf);
            `ulog("In_ReadCount   :" @ In_ReadCount);

            // TCP buffer empty, try again next tick.
            if (In_ReadCount == 0)
            {
                return;
            }

            if (In_ReadCount != In_NumToRead)
            {
                `uerror("Expected" @ In_NumToRead @ "bytes, got" @ In_ReadCount);
                ReadState = ERS_WaitingForHeader;
                break;
            }

            for (In_RecvBufIdx = 0; In_RecvBufIdx < In_ReadCount; ++In_RecvBufIdx)
            {
                RecvMsgBufMulti[In_MsgBufIdx++] = RecvMsgBuf[In_RecvBufIdx];
            }

            if (In_Part < MULTI_PART_END)
            {
                ReadState = ERS_WaitingForMultiHeader;
            }
            else if (In_Part == MULTI_PART_END)
            {
                ++NumMessagesReceived;
                ReadState = ERS_WaitingForHeader;
            }
            else
            {
                `uerror("unexpected part:" @ In_Part @ "in state:" @ ReadState);
                ReadState = ERS_WaitingForHeader;
            }

            break;

        default:
            `uerror("invalid ReadState:" @ ReadState);
            return;
        }
    }

        // If multi-msg flag not set:
        //   1. read header bytes
        //   2. parse header
        //      2.1 if single/static msg
        //          2.1.1 read message bytes
        //      2.2 else if multipart msg
        //          2.2.2 set multi-msg flag
        //          2.2.3 read message bytes
        // Else if multi-msg flag set:
        //   1. read header bytes  // TODO: can we get non-header bytes here?
        //   2. parse header
        //   3. read message bytes
        //   4. if no more expected (part end marker), set multi-msg flag off
}

final function ConnectToServer()
{
    LinkMode = MODE_Binary;
    ReceiveMode = RMODE_Manual;

    // TODO: do these do anything for binary?
    InLineMode = LMODE_UNIX;
    OutLineMode = LMODE_UNIX;

    Resolve(TargetHost);
}

// TODO: dedicated function for sending static bytes.

// TODO: take Size, Part, MessageType as arguments here?
// TODO: REALLY CONSIDER ADDING HEADERS TO MULTIPART MESSAGE ENCODING
//       ALREADY IN THE ENCODING PHASE. THIS IS A LOT OF ADDED COMPLEXITY
//       TO PERFORM THE PACKAGE SPLITTING HERE.
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
        `uerror("invalid NumPartsOut:" @ NumPartsOut);
        return;
    }

    // The easy case.
    // TODO: validate that more than PACKET_SIZE
    //   bytes are not trying to be sent in a single part?
    if (NumPartsOut == 1 || Bytes.Length <= PACKET_SIZE)
    {
        for (ByteIdx = 0; ByteIdx < Bytes.Length; ++ByteIdx)
        {
            OutBuf[ByteIdx] = Bytes[ByteIdx];
        }

        OutBuf[1] = SINGLE_PART;

        NumToSend = Bytes.Length;
        NumSent = SendBinary(NumToSend, OutBuf);
        `ulog("Sent" @ NumSent @ "bytes");
        return;
    }

    // Multipart messages.
    // Skip first header.
    ByteIdx = 4;

    // Cache MessageType, should be re-used as such in
    // all multipart message packets.
    MTOut0 = Bytes[2];
    MTOut1 = Bytes[3];

    BytesSent = 0;
    for (PartOut = 0; PartOut < NumPartsOut; ++PartOut)
    {
        if (PartOut == (NumPartsOut - 1))
        {
            PartOut = MULTI_PART_END;
        }

        // Write size later.
        OutBufIdx = 1;
        OutBuf[OutBufIdx++] = PartOut;
        OutBuf[OutBufIdx++] = MTOut0;
        OutBuf[OutBufIdx++] = MTOut1;
        while ((OutBufIdx < PACKET_SIZE) && (ByteIdx < Bytes.Length))
        {
            OutBuf[OutBufIdx++] = Bytes[ByteIdx++];
        }
        NumToSend = OutBufIdx;
        OutBuf[0] = NumToSend;

        NumSent = SendBinary(NumToSend, OutBuf);
        BytesSent += NumSent;
        `ulog("Sent" @ NumSent @ "bytes,"
            @ "part:" @ PartOut
            @ "NumToSend:" @ NumToSend
            @ "BytesSent:" @ BytesSent);

        // `ulog("Buffer:" @ class'UMBTestsMutator'.static.StaticBytesToString(OutBuf, NumToSend));
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

// WARNING: this gives garbage if packet size == 255. OR DOES IT?
// TODO: C++ server side had some bugs, should try out how this
//       works now that they are fixed.
// event ReceivedBinary(int Count, byte B[PACKET_SIZE])
// {
// }

DefaultProperties
{
    TargetHost="127.0.0.1"
    TargetPort=55555

    NumMessagesReceived=0
    ReadState=ERS_WaitingForHeader

    TickGroup=TG_DuringAsyncWork
}
