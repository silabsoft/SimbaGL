/**
* Copyright 2010 by Benjamin J. Land (a.k.a. BenLand100)
*
* This file is part of the SMART Minimizing Autoing Resource Thing (SMART)
*
* SMART is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* SMART is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with SMART. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _MAIN_H
#define _MAIN_H

#include <windows.h>
extern "C" bool __stdcall DllMain(HINSTANCE, int, void*);


//These are intended for use with the SCAR/Simba plugin loaders, and nothing else.
extern "C" long __stdcall GetFunctionCount();
extern "C" long __stdcall GetFunctionInfo(int, void*&, char*&);

bool internalConstructor();
void internalDestructor();
DWORD* requestSharedMemory();
bool isSharedMemoryBusy();
bool isSharedMemoryReturned();
bool isGoingToCallBack();
#endif /* _MAIN_H */