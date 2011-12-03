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

	/* Aftermath: updated this function to 1) resturn a success boolean 2) use a LongWord to avoid overflow
	 * 3) put x, y first to maintain uniformity with Simba/SCAR functions 4) properly use by-reference in Pascal
	 * to avoid access violations (e.g. people passing in numbers instead of variables) */
	(char *) "GetModelPositionByChecksum", (char *) "function InterceptionGetModelPositionByChecksum(var x, y: Integer; id: LongWord): Boolean;",
	(char*)"GetModelPositionByTriangleCount", (char*)"procedure InterceptionGetModelPositionByTriCount(triCount :integer; var x,y : integer)",
	(char *) "FindInventoryFirst", (char *) "function InterceptionFindInventoryFirst(var x, y: Integer; id: LongWord): Boolean;",
	
	// (char*)"TestRecordType", (char*)"procedure TestRecordType(rec: TModel)",
};


#define NumExports 9
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
bool WINAPI DllMain(HINSTANCE instance, int reason, void* checks) {
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

/*
 * Aftermath: will need to fix the leaks in this stuff later.
 * This is useful for defining our custom types to use on the pascal side.
 * from yakman
 * http://villavu.com/forum/showthread.php?t=39677
 * dest - passed by GetTypeInfo()
 * src - what the string should contain, regular c string
 */

/*
typedef struct delphi_string {
	int ref;
	int length;
	char bytes[1];
} delphi_string;

void
make_delphi_string(char** dest, char* src) {
	
	char* buffer = new char[256];

	*dest = buffer + 8;
	delphi_string* str = (delphi_string*)buffer;
	str->ref = 255;
	str->length = strlen(src);
	strcpy(str->bytes, src);
	str->bytes[str->length + 1] = 0; //double null
}

int __stdcall GetTypeCount(void) {
	return 0;
	//return 1;
}

int __stdcall GetTypeInfo(int x, char** type_name, char** type_def) {

	switch(x) {
		// Aftermath: add other types in other cases
		case 0: {
			make_delphi_string(type_name, "TModel");
			make_delphi_string(type_def, "record CRC: Longword; x, y: Word; end;");
				return x;
		} break;
	}
	return -1;
}
// end yakman-related section */


//sets the debug status
void setDebug(bool d){
	debug = d;
}

//returns debug status
bool isDebug(){
	return debug;
}

void __stdcall TestRecordType(void *test) {
	unsigned long *crc = (unsigned long *) test;
	unsigned short *x = (unsigned short *) (crc + 1);
	unsigned short *y = x + 1;

	wcout << "crc: " << *crc << "x: " << *x << "y: " << *y << endl;
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
hMapFile = OpenFileMappingA(
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