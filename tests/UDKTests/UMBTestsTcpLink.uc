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

/*
var private int ClientPort;

var private array<string> Responses;

var private int TransactionID;
var private array<string> TransactionStack;

var bool bDone;

struct PendingCheck
{
    var string TestName;
    var int TransactionID;
    var string GMPOperandName;
    var array<int> BigIntOperand;
};

var private array<PendingCheck> PendingChecks;
var private PendingCheck CurrentCheck;

var string TargetHost;
var int TargetPort;

var private array<string> R_Array;
var private string R_TID;
var private string R_GMPOperandName;
var private string R_Result;
var private array<byte> R_ResultBytes;

var private int Failures;
var private int LastFailuresLogged;

var private int ChecksDone;
var private int RequiredChecks;

const ID_PRIME = "PRIME";
const TID_PRIME = "TPRIME";

var delegate<OnRandPrimeReceived> RandPrimeDelegate;
delegate OnRandPrimeReceived(const out array<byte> P);

simulated event Tick(float DeltaTime)
{
    local int I;
    // local int J;
    // local int K;
    // local int LenResult;
    // local string ByteS;
    local int Fail;

    for (I = 0; I < Responses.Length; ++I)
    {
        if (Responses[I] == "SERVER_ERROR")
        {
            `ulog("##ERROR##: SERVER ERROR!");
            PendingChecks.Remove(0, 1); // TODO: should do checks in a loop?
            ++ChecksDone;
            ++Failures;
            continue;
        }

        ParseStringIntoArray(Responses[I], R_Array, " ", False);
        R_TID = R_Array[0];
        R_GMPOperandName = R_Array[1];
        R_Result = R_Array[2];

        if (Len(R_Result) < 3)
        {
            R_ResultBytes.Length = 1;
            R_ResultBytes[0] = class'WebAdminUtils'.static.FromHex(R_Result);
        }
        else
        {
            // LenResult = Len(R_Result);
            // if ((LenResult % 2) != 0)
            // {
            //     R_Result = "0" $ R_Result;
            //     ++LenResult;
            // }
            // K = 0;
            // J = 0;
            // R_ResultBytes.Length = LenResult / 2;
            // while (J < LenResult)
            // {
            //     ByteS = Mid(R_Result, J, 2);
            //     R_ResultBytes[K++] = class'WebAdminUtils'.static.FromHex(ByteS);
            //     J += 2;
            // }
            class'FCryptoBigInt'.static.BytesFromHex(R_ResultBytes, R_Result);
        }

        if (R_TID == ID_PRIME)
        {
            RandPrimeDelegate(R_ResultBytes);
            continue;
        }

        // `ulog(Responses[I]);
        // class'FCryptoTestMutator'.static.LogBytes(R_ResultBytes);

        CurrentCheck = PendingChecks[0];

        Fail = class'FCryptoTestMutator'.static.CheckEqz(
            CurrentCheck.BigIntOperand,
            R_ResultBytes
        );

        if (Fail > 0)
        {
            `ulog("##ERROR##: CurrentCheck.TestName       :" @ CurrentCheck.TestName);
            `ulog("##ERROR##: CurrentCheck.TID            :" @ CurrentCheck.TransactionID);
            `ulog("##ERROR##: CurrentCheck.GMPOperandName :" @ CurrentCheck.GMPOperandName);
            `ulog("##ERROR##: R_TID                       :" @ R_TID);
            `ulog("##ERROR##: R_Result                    :" @ R_Result);
            `ulog("##ERROR##: R_GMPOperandName            :" @ R_GMPOperandName);
            `ulog("##ERROR##: Response                    :" @ Responses[I]);
            Failures += Fail;
        }

        PendingChecks.Remove(0, 1); // TODO: should do checks in a loop?
        ++ChecksDone;
    }

    if (I > 0)
    {
        Responses.Remove(0, I);
    }

    if (Failures > 0 && LastFailuresLogged < (Failures - 1))
    {
        `ulog("##ERROR##: ---" @ Failures @ "FAILURES DETECTED! ---");
        LastFailuresLogged = Failures;
    }

    if (ChecksDone > 0 && (ChecksDone % 25) == 0)
    {
        `ulog("---" @ ChecksDone @ "checks done so far. ---");
    }

    if (ChecksDone > 0 && (ChecksDone >= RequiredChecks))
    {
        `ulog("--- ALL CHECKS DONE" @ ChecksDone $ "/" $ RequiredChecks @ "---");
        ChecksDone = 0;
        RequiredChecks = 0;

        if (Failures > 0)
        {
            `ulog("##ERROR##: ---" @ Failures @ "FAILURES DETECTED! ---");
        }
    }

    super.Tick(DeltaTime);
}

final simulated function ConnectToServer()
{
    LinkMode = MODE_Binary;
    ReceiveMode = RMODE_Event;
    Resolve(TargetHost);
}

final simulated function Begin()
{
    ++TransactionID;
}

final simulated function End()
{
    local string Str;
    local int I;

    // Actually push the stuff here.
    Str = "T" $ TransactionID;
    for (I = 0; I < TransactionStack.Length; ++I)
    {
        Str @= "[" $ TransactionStack[I] $ "]";
    }

    // `ulog("sending" @ TransactionID);
    SendText(Str);
    TransactionStack.Length = 0;
}

final simulated function Var(string VarName, string Value)
{
    TransactionStack.AddItem("var" @ VarName @ "'" $ Value $ "'");
}

final simulated function Op(string Op, string Dst, string A, string B)
{
    TransactionStack.AddItem("op" @ Op @ Dst @ A @ B );
}

final simulated function Eq(string GMPOperandName, const out array<int> B,
    string TestName = "Unnamed")
{
    local PendingCheck Check;

    Check.TransactionID = TransactionID;
    Check.GMPOperandName = GMPOperandName;
    Check.BigIntOperand = B;

    PendingChecks.AddItem(Check);
    RequiredChecks = PendingChecks.Length;
}

final simulated function RandPrime(int Size)
{
    SendText(
        TID_PRIME
        @ "[var s '" $ ToHex(Size) $ "']"
        @ "[var x '0']"
        @ "[op rand_prime x s s]"
    );
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

    `ulog("Count:" @ Count);

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
    bDone=False

    ChecksDone=0
    Failures=0
    LastFailuresLogged=0

    TargetHost="127.0.0.1"
    TargetPort=65432
    TransactionID=0

    TickGroup=TG_DuringAsyncWork
}
*/
