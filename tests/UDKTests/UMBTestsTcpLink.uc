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

var private int ClientPort;

var string TargetHost;
var int TargetPort;

var int Idx;
var int NumSent;
var int NumToSend;
var byte OutBuf[255];

final function ConnectToServer()
{
    LinkMode = MODE_Binary;
    ReceiveMode = RMODE_Event;
    Resolve(TargetHost);
}

// TODO: dedicated function for sending static bytes.

final function SendBytes(
    const out array<byte> Bytes)
{
    // The easy case.
    if (Bytes.Length <= 255)
    {
        for (Idx = 0; Idx < Bytes.Length; ++Idx)
        {
            OutBuf[Idx] = Bytes[Idx];
        }

        NumToSend = Bytes.Length;
        NumSent = SendBinary(NumToSend, OutBuf);
        `ulog("Sent" @ NumSent @ "bytes");
        return;
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

event ReceivedBinary(int Count, byte B[255])
{
    local byte Size;
    local byte Part;
    local int MessageType;
    local int I;

    `ulog("Count       :" @ Count);

    Size = B[0];
    Part = B[1];
    MessageType = B[2] | (B[3] << 8);

    `ulog("Size        :" @ Size);
    `ulog("Part        :" @ Part);
    `ulog("MessageType :" @ MessageType);

    I = 4;
}

DefaultProperties
{
    TargetHost="127.0.0.1"
    TargetPort=55555

    TickGroup=TG_DuringAsyncWork
}
