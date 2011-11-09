#include "Main.h"
#include "GLFunctions.h"
#include <cstring>
#include <cstdlib>
#include <iostream>
using namespace std;
static char* exports[] = {
	(char*)"setDebug", (char*)"procedure InterceptionSetDebug(d: boolean);",
	(char*)"isDebug", (char*)"function InterceptionGetDebug() : boolean;",
	(char*)"setOverlay", (char*)"procedure InterceptionSetOverlay(b: boolean);",
	(char*)"getViewport","function InterceptionGetViewport(var x,y,width,height : integer) : boolean;",
	(char*)"getGLPosition","procedure InterceptionGetGLPosition(x,y : integer; var posX,posY,posZ : double);",
	(char*)"SetUsingResizeableClient", (char*)"procedure InterceptionSetUsingResizeableClient(b: boolean);",
	(char*)"GetModelPositionByChecksum", (char*)"procedure InterceptionGetModelPositionByChecksum(id :integer; var x,y : integer);"
};


#define NumExports 7
HMODULE dllinst;
bool debug = false;
DWORD * pCommands;
char szSharedMemoryName[]="Local\\InterceptionMappingObject";


//STDCALL - SCAR/Simba
long __stdcall  GetFunctionCount() {
    return NumExports;
}

//STDCALL - SCAR/Simba
long __stdcall GetFunctionInfo(int index, void*& address, char*& def) {
    if (index < NumExports) {
        address = (void*)GetProcAddress(dllinst, exports[index*2]);
        strcpy(def, exports[index*2+1]);
        return index;
    }
    return -1;
}

//Windows DLL entrypoint/exitpoint
bool DllMain(HINSTANCE instance, int reason, void* checks) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            dllinst = instance;
			cout << "Starting Interception 0.2 \n";
            return internalConstructor();
        case DLL_THREAD_ATTACH:
            return true;
        case DLL_PROCESS_DETACH:
            cout << "Interception Finalized!\n";
            return true;
        case DLL_THREAD_DETACH:
            return true;
	}
    return false;
}


//sets the debug status
void setDebug(bool d){
	debug = d;
}

//returns debug status
bool isDebug(){
	return debug;
}

void setOverlay(bool b){
	DWORD * arr = requestSharedMemory();
	arr[0] = 2;
	arr[2] = 4;
	arr[3] = b ? 1 : 0;
	arr[1] = 1;
	if(debug){
		cout << "setOverlay: ";
		cout << b;
		cout << "\n";
	}
}
int fail = 0;
DWORD* requestSharedMemory(){
	while(fail < 500){
		if(isSharedMemoryBusy()){
			Sleep(10);		
			fail++;
		}
		else
			break;
	}
	return pCommands;
}
bool isSharedMemoryBusy(){
	return pCommands[1] > 0 ? true : false;
}
int noHesNot;
bool isGoingToCallBack(){
	while(noHesNot < 500){
		if(isSharedMemoryReturned()){
			noHesNot = 0;
			return true;
		}
		if(!isSharedMemoryBusy()){
			noHesNot = 0;
			return false;
		}
		Sleep(10);
		noHesNot++;	
	}
		noHesNot = 0;
		return false;
}
bool isSharedMemoryReturned(){
	if(pCommands[1] == 2)
		return true;
	else
		return false;
}


//loads the shared memory for communication.
bool internalConstructor(){
	HANDLE hMapFile;
hMapFile = OpenFileMapping(
                   FILE_MAP_ALL_ACCESS,   // read/write access
                   FALSE,                 // do not inherit the name
                   szSharedMemoryName);               // name of mapping object
	if (hMapFile == NULL)
	{
		cout << "Could not create file mapping object \n";
		return false;
	}
	pCommands = (DWORD *)MapViewOfFile(hMapFile,   // handle to map object
							FILE_MAP_ALL_ACCESS, // read/write permission
							0,
							0,
							255);
	if (pCommands == NULL)
	{
			cout << "Could not map view of file (%d). \n";
		CloseHandle(hMapFile);
		return false;
	}
	cout << "Running Interception \n";
	return true;
}

//closes the shared memory.
void internalDeconstructor(){

}