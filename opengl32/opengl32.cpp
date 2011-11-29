#include <time.h>
#include <vector>
using namespace std;

#include "opengl32.h"
#include "uifunction.h"

#define GL_TEXTURE_RECTANGLE_ARB 0x84F5

bool drawing_model;
bool draw = false;
bool draw_overlay = false;
bool resizeableClient = false;
bool logging = false;

/* Aftermath: stride 24 is used to draw items on the ground
  (along with a bunch of other miscelannea). [I was using the fixed-size
  client.] */
unsigned int draw_stride = 12;
unsigned int count = 0;

struct Model;
/* Aftermath: New model-finding implementation.
 * currentModels: Models that are drawn in the current frame.
 * newModels: Models that are to be drawn in the next frame.
 * When the frame is switched, currentModels is set to point to newModels
 * and the old list is destroyed.
 */
vector<Model *> *currentModels = NULL;
vector<Model *> *newModels = NULL;

/* Aftermath: Regarding synchronization - whenever modifying either the
 * currentModels or newModels pointer to a vector<Model *>, any of the
 * elements therein, or accessing any element pointed to by any pointers
 * of the vector [this is the one that is sometimes subtle], you have to
 * start the accessing code with EnterCriticalSection(csXXXXModels) and
 * leave the accessing code with LeaveCriticalSection(csXXXXModels). If
 * the code modifies both, you MUST enter current, and then new, and leave
 * new, and then current. This MUST be done in this order - if done in the
 * wrong order, it will lead to deadlock, and if not done at all, it will
 * lead to memory access violations.
 *
 */
CRITICAL_SECTION csCurrentModels;
CRITICAL_SECTION csNewModels;

vector<Model> models;

struct Model
{
	GLfloat x;				//x,y,z coords
	GLfloat y;
	GLfloat z;
	unsigned int stride;
	unsigned int x_s;		//screen coord
	unsigned int y_s;		//screen coord
	unsigned int id;		//id of model
	unsigned int count;
	unsigned int triangles;
	bool firstFirst;
};

/* Aftermath: The C-side counterpart to the TModel provided to SCAR/Simba.
 * Note: Not actually in use, just something I was toying around with. */
struct TModel
{
	unsigned long crc;
	unsigned short x, y;
};

// vector<Model> models;

GLuint lastBuffer = 0;

//vector<unsigned int> bufferCRC;

unsigned int bufferCRC[50000];

HDC				hDC;
HFONT			hOldFont;
HFONT			hFont;
UINT			FontBase;
bool            bFontsBuild = 0;

char szSharedMemoryName[]="Local\\InterceptionMappingObject";
DWORD * pCommands;

unsigned int last_stride;

void BuildFonts()
{
	hDC			= wglGetCurrentDC();

	FontBase	= glGenLists(96);
	hFont		= CreateFont(-10, 0, 0, 0, FW_NORMAL,	FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FF_DONTCARE|DEFAULT_PITCH, "Courier"); //"Lucida Console");
	hOldFont	= (HFONT) SelectObject(hDC, hFont);
	wglUseFontBitmaps(hDC, 32, 96, FontBase);
	SelectObject(hDC, hOldFont);
	DeleteObject(hFont);

	bFontsBuild = true;
}

void CreateSharedMemory()
{
	srand ( time(NULL) );
	HANDLE hMapFile;

	hMapFile = CreateFileMapping(
		INVALID_HANDLE_VALUE,    // use paging file
		NULL,                    // default security
		PAGE_READWRITE,          // read/write access
		0,                       // maximum object size (high-order DWORD)
		255,                     // maximum object size (low-order DWORD)
		szSharedMemoryName);     // name of mapping object

	if (hMapFile == NULL)
	{
		add_log("Could not create file mapping object (%d)",GetLastError());
		return;
	}
	pCommands = (DWORD *)MapViewOfFile(hMapFile,   // handle to map object
		FILE_MAP_ALL_ACCESS, // read/write permission
		0,
		0,
		255);
	if (pCommands == NULL)
	{
		add_log("Could not map view of file (%d).\n",GetLastError());
		CloseHandle(hMapFile);
		return;
	}
}

void ExecuteCommands()
{ //pCommands[2] is going to be a length indicator when I get to figure out how to get array's passed from pascal to C++

	if(pCommands[1] == 1)		//if command status "not done"
	{
		if(pCommands[0] == 1)	//if command = FindModelByID
		{
			const int req_id = pCommands[3];
			vector<Model *> matching;

			EnterCriticalSection(&csCurrentModels);
			EnterCriticalSection(&csNewModels);

			/* Aftermath: I made a modificati on to randomly choose a satisfactory model
			 * satisfying the CRC. This way, if the first one happens to be fenced off
			 * or summat, the script can keep going.
			 */
			if(currentModels) {
				vector<Model *>::iterator it = currentModels->begin();
				for(; it != currentModels->end(); it++) {
					if((*it)->id == req_id) {
						matching.push_back(*it);
					}
				}

				if(matching.size()) {
					// MessageBoxA(NULL, "HIA", "HI", 0);
					const int selectedId = rand() % matching.size();

//					Model *copy = new Model;

					/* Aftermath: this scope is here to avoid using any of selectedModel's
					 * fields, which would cause an access violation when it is deleted.
					 */
//					{
						const Model * const selectedModel = matching[selectedId];
//						*copy = *selectedModel;
//						simbaModels.push_back(copy);
//					}


					pCommands[3] = selectedModel->x_s;
					pCommands[4] = selectedModel->y_s;
					pCommands[1] = 2;

					char * store = (char *) malloc(sizeof(char) * 200);
					itoa(req_id, store, 10);
					// MessageBoxA(NULL, store, "SEARCHED THROUGH MODELES:", 0);
					//MessageBoxA(NULL, store, "FOUND", 0);
				} else {
					//MessageBoxA(NULL, "SAD", "NESS", 0);
				}
			}
			LeaveCriticalSection(&csNewModels);
			LeaveCriticalSection(&csCurrentModels);
			/*for (int i = 0; i < models.size(); i++)
			{
				if (models[i].id == pCommands[3])
				{
					pCommands[3] = models[i].x_s;	//xcoord
					pCommands[4] = models[i].y_s;	//ycoord
					pCommands[1] = 2;	//set command status to response
				}
			}*/
		}
		if(pCommands[0] == 2){ //set overlay
			draw_overlay = (pCommands[3] == 1? true : false) ;
		}
		if(pCommands[0] == 3){ // get viewport
			int Viewport[4];
			orig_glGetIntegerv(GL_VIEWPORT, Viewport);
			pCommands[3] = Viewport[0];
			pCommands[4] = Viewport[1];
			pCommands[5] = Viewport[2];
			pCommands[6] = Viewport[3];
			pCommands[1] = 2;
		}
		if(pCommands[0] == 4){ //get GL Position
			double x, y ,z;
			MouseCoordinateToGLPos(pCommands[3], pCommands[4],x,y,z,resizeableClient);
			pCommands[3] = x;
			pCommands[4] = y;
			pCommands[5] = z;
			pCommands[1] = 2;
		}
		if(pCommands[0] == 5){ //using ResizeableClient?
			resizeableClient = (pCommands[3] == 1? true : false) ;
		}
		if(pCommands[0] == 6){ //FindModelByTriangle
			for (int i = 0; i < models.size(); i++)
			{
				if (models[i].triangles == pCommands[3])
				{
					pCommands[3] = models[i].x_s;	//xcoord
					pCommands[4] = models[i].y_s;	//ycoord
					pCommands[1] = 2;	//set command status to response
				}
			}
		}
	}
	if(pCommands[1] != 2) //we have read the command and are not required to respond
		pCommands[1] = 0;
}


void glPrint(int x, int y, const char *fmt, ...)
{
	if (!bFontsBuild) BuildFonts();

	if (fmt == NULL)	return;

	glRasterPos2i(x,y);

	char		text[256];
	va_list		ap;

	va_start(ap, fmt);
	vsprintf(text, fmt, ap);
	va_end(ap);

	glPushAttrib(GL_LIST_BIT);
	glListBase(FontBase - 32);
	glCallLists(strlen(text), GL_UNSIGNED_BYTE, text);
	glPopAttrib();
}

DWORD QuickChecksum(DWORD *pData, int size)
{
	if(!pData) { return 0x0; }

	DWORD sum;
	DWORD tmp;
	sum = *pData;

	for(int i = 1; i < (size/4); i++)
	{
		tmp = pData[i];
		tmp = (DWORD)(sum >> 29) + tmp;
		tmp = (DWORD)(sum >> 17) + tmp;
		sum = (DWORD)(sum << 3)  ^ tmp;
	}

	return sum;
}



void sys_glMultiTexCoord2fARB(GLenum target, GLfloat s, GLfloat t)
{
	if(logging)
		add_log("glMultiTexCoord2fARB %d %d %d", target,s,t);
	orig_glMultiTexCoord2fARB(target,s,t);
}

void sys_glActiveTextureARB(GLenum target)
{
	if(logging)
		add_log("glActiveTextureARB %s ", GLenumToString(target));
	orig_glActiveTextureARB(target);
}

void sys_glBufferDataARB(GLenum target, GLsizei size, const void* data, GLenum usage)
{
	if(logging)
		add_log("BufferDataARB %d %d data %d", target,size,usage);
	//	if(target == 0x8892)

	//bufferCRC.reserve(lastBuffer+10);
	bufferCRC[lastBuffer] = QuickChecksum((DWORD*)data, size);
	//bufferCRC[lastBuffer] = lastBuffer;

	orig_glBufferDataARB(target, size, data, usage);
}
void sys_glVertexPointer (GLint size,  GLenum type,  GLsizei stride,  const GLvoid *pointer)
{
	if(logging)
		add_log("glVertexPointer %d %d %d pointer", size,type,stride);
	last_stride = stride;
	drawing_model = true;
	if(size == 3 && type == GL_FLOAT && pointer != 0)		//Model model
	{
		// Aftermath: modified to allocate on heap so

		Model *newModel = new Model;
		GLfloat *temp;

		bool random_vertex = !draw_overlay;
		int model_vertex = 0;

		//Silab: we only choose random vert when we are not displaying debug for ease of reading purposes.
		if(random_vertex) {
			//Silab: although I do not have a way of grabbing vertex count I am asuming every model has atleast 100 verts.
			model_vertex = rand() % 100;
		}

		// Aftermath: generalized to use stride instead of 12
		int vertex_offset = model_vertex * stride;
		newModel->x = (DWORD) pointer + vertex_offset;
		newModel->y = (DWORD) pointer + 4 * vertex_offset;
		newModel->z = (DWORD) pointer + 8 * vertex_offset;
		newModel->stride = stride;

		models.push_back(*newModel);
	}
	else
	{
		Model newModel;
		newModel.x = 0;
		newModel.y = 0;
		newModel.z = 0;
		newModel.stride = stride;
		models.push_back(newModel);
	}
	(*orig_glVertexPointer) (size, type, stride, pointer);
}

void sys_glDrawElements (GLenum mode,  GLsizei count,  GLenum type,  const GLvoid *indices)
{
	if(logging)
		add_log("glDrawElements %d %d %d indices",mode,count,type);
	if(drawing_model){
		models.back().id = bufferCRC[lastBuffer];
		models.back().triangles = count/3;
		if(last_stride == draw_stride && draw_overlay)
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		else
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	else{
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	(*orig_glDrawElements) (mode, count, type, indices);
}
void sys_glDrawArrays(GLenum  mode,  GLint  first,  GLsizei  count){
	if(logging)
		add_log("glDrawArrays %d %d %d",mode,first,count);
	(*orig_glDrawArrays) (mode,first,count);
}
void sys_glBindBufferARB(GLenum target, GLuint id)
{
	if(logging)
		add_log("glBindBufferARBs %d %d",target,id);
	lastBuffer = id;

	orig_glBindBufferARB(target, id);
}

void sys_glPopMatrix (void)
{
	if(logging)
		add_log("glPopMatrix");
	double ModelView[16];
	double ProjView[16];
	int Viewport[4];
	double View2D[3];

	if(drawing_model)
	{
		orig_glGetDoublev(GL_MODELVIEW_MATRIX, ModelView);
		orig_glGetDoublev(GL_PROJECTION_MATRIX, ProjView) ;
		if(resizeableClient){
			orig_glGetIntegerv(GL_VIEWPORT, Viewport);
		}
		else{
			Viewport[0] = 4;
			Viewport[1] = 4;
			Viewport[2] = 509;
			Viewport[3] = 333;
		}



		if(gluProject(models.back().x, models.back().y, models.back().z, ModelView, ProjView, Viewport, &View2D[0], &View2D[1], &View2D[2]) == GL_TRUE)
		{
			models.back().x_s = (unsigned int)View2D[0];
			models.back().y_s = (unsigned int)Viewport[3]-(unsigned int)View2D[1];
		}
		
		EnterCriticalSection(&csNewModels);
		if(newModels) {
			if(models.size()) {
				Model *copy = new Model;
				*copy = models.back();
				newModels->push_back(copy);
			}
		}
		LeaveCriticalSection(&csNewModels);
	}

	drawing_model = false;

	(*orig_glPopMatrix) ();
}

void sys_glOrtho (GLdouble left,  GLdouble right,  GLdouble bottom,  GLdouble top,  GLdouble zNear,  GLdouble zFar)
{
	if(logging)
		add_log("glOrtho %f %f %f %f %f %f",left,right,bottom,top,zNear,zFar);
	(*orig_glOrtho) (left, right, bottom, top, zNear, zFar);
}
int dataDisplay = 0;
bool drawAll;

LPSTR GLenumToString(GLenum cap){
	LPSTR name;
	switch(cap){
	case 32879:
		name = "GL_TEXTURE_3D";
		break;
	case 0x8513:
		name = "GL_TEXTURE_CUBE_MAP";
		break;
	case 0x0C60:
		name = "GL_TEXTURE_GEN_S";
		break;
	case 0x0C61:
		name = "GL_TEXTURE_GEN_T";
		break;
	case 0x0C62:
		name = "GL_TEXTURE_GEN_R";
		break;
	case 0x0C63:
		name = "GL_TEXTURE_GEN_Q";
		break;
	case 0x8620:
		name ="GL_VERTEX_PROGRAM";
		break;
	case 0x0DE1:
		name = "GL_TEXTURE_2D";
		break;
	case 0x0BE2:
		name = "GL_BEND";
		break;
	case 0x0BC0:
		name = "GL_ALPHA_TEST";
		break;
	case 0x0B50:
		name = "GL_LIGHTING";
		break;
	case 0x0B71:
		name ="GL_DEPTH_TEST";
		break;
	case 0x0B44:
		name ="GL_CULL_FACE";
		break;
	case 0x0B57:
		name ="GL_COLOR_MATERIAL";
		break;
	case 0x4000:
		name ="GL_LIGHT0";
		break;
	case 0x4001:
		name ="GL_LIGHT1";
		break;
	case 0x0C11:
		name = "GL_SCISSOR_TEST";
		break;
	case 0x84F5:
		name="GL_TEXTURE_RECTANGLE";
		break;
	case 0x809D:
		name = "GL_MULTISAMPLE";
		break;
	case 0x0DE0:
		name = "GL_TEXTURE_1D";
		break;
	case 0x8064:
		name = "GL_PROXY_TEXTURE_2D";
		break;	
	case 0x8515:
		name = "GL_TEXTURE_CUBE_MAP_POSITIVE_X";
		break;	
	case 0x8516: 
		name = "GL_TEXTURE_CUBE_MAP_NEGATIVE_X";
		break;	
	case 0x8517:
		name = "GL_TEXTURE_CUBE_MAP_POSITIVE_Y";
		break;	
	case 0x8518:
		name = "GL_TEXTURE_CUBE_MAP_NEGATIVE_Y";
		break;	
	case 0x8519:
		name = "GL_TEXTURE_CUBE_MAP_POSITIVE_Z ";  
		break;	
	case 0x851A:
		name = "GL_TEXTURE_CUBE_MAP_NEGATIVE_Z";
		break;	
	case 0x851B:
		name = "GL_PROXY_TEXTURE_CUBE_MAP";      
		break;	

	case GL_COLOR_INDEX:
		name = "GL_COLOR_INDEX";
		break;
	case GL_RED:
		name = "GL_RED";
		break;
	case GL_GREEN:
		name = "GL_GREEN";
		break;
	case GL_BLUE:
		name = "GL_BLUE";
		break;
	case GL_ALPHA:
		name = "GL_ALPHA";
		break;
	case GL_RGB:
		name = "GL_RGB";
		break;
		/*case GL_BGR:
		name = "GL_BGR";
		break;
		*/
	case GL_RGBA:
		name = "GL_RGBA";
		break;
		/*case GL_BGRA:
		name = "GL_BGRA";
		break;
		*/
	case GL_LUMINANCE:
		name = "GL_LUMINANCE";
		break;
	case GL_LUMINANCE_ALPHA:
		name = "GL_LUMINANCE_ALPHA";
		break;
	case GL_UNSIGNED_BYTE:
		name = "GL_UNSIGNED_BYTE";
		break;
	case GL_BYTE:
		name = "GL_BYTE";
		break;
	case GL_BITMAP:
		name = "GL_BITMAP";
		break;
	case GL_UNSIGNED_SHORT:
		name = "GL_UNSIGNED_SHORT";
		break;
	case GL_SHORT:
		name = "GL_SHORT";
		break;
	case GL_UNSIGNED_INT:
		name = "GL_UNSIGNED_INT";
		break;
	case GL_INT:
		name = "GL_INT";
		break;
	case GL_FLOAT:
		name = "GL_FLOAT";
		break;
		/*
		case GL_UNSIGNED_BYTE_3_3_2:
		name = "GL_UNSIGNED_BYTE_3_3_2";
		break;
		case GL_UNSIGNED_BYTE_2_3_3_REV:
		name = "GL_UNSIGNED_BYTE_2_3_3_REV";
		break;
		case GL_UNSIGNED_SHORT_5_6_5:
		name = "GL_UNSIGNED_SHORT_5_6_5";
		break;
		case GL_UNSIGNED_SHORT_5_6_5_REV:
		name = "GL_UNSIGNED_SHORT_5_6_5_REV";
		break;
		case GL_UNSIGNED_SHORT_4_4_4_4:
		name = "GL_UNSIGNED_SHORT_4_4_4_4";
		break;
		case GL_UNSIGNED_SHORT_4_4_4_4_REV:
		name = "GL_UNSIGNED_SHORT_4_4_4_4_REV";
		break;
		case GL_UNSIGNED_SHORT_5_5_5_1:
		name = "GL_UNSIGNED_SHORT_5_5_5_1";
		break;
		case GL_UNSIGNED_SHORT_1_5_5_5_REV:
		name = "GL_UNSIGNED_SHORT_1_5_5_5_REV";
		break;
		case GL_UNSIGNED_INT_8_8_8_8:
		name = "GL_UNSIGNED_INT_8_8_8_8";
		break;
		case GL_UNSIGNED_INT_8_8_8_8_REV:
		name = "GL_UNSIGNED_INT_8_8_8_8_REV";
		break;
		case GL_UNSIGNED_INT_10_10_10_2:
		name = "GL_UNSIGNED_INT_10_10_10_2";
		break;
		case GL_UNSIGNED_INT_2_10_10_10_REV:
		name = "GL_UNSIGNED_INT_2_10_10_10_REV";
		*/
	case GL_POINTS:
		name = "GL_POINTS";
		break;
	case GL_LINES:
		name = "GL_LINES";
		break;
	case GL_LINE_STRIP:
		name = "GL_LINE_STRIP";
		break;
	case GL_LINE_LOOP:
		name = "GL_LINE_LOOP";
		break;
	case GL_TRIANGLES:
		name = "GL_TRIANGLES";
		break;
	case GL_TRIANGLE_STRIP:
		name = "GL_TRIANGLE_STRIP";
		break;
	case GL_TRIANGLE_FAN:
		name = "GL_TRIANGLE_FAN";
		break;
	case GL_QUADS:
		name = "GL_QUADS";
		break;
	case GL_QUAD_STRIP:
		name = "GL_QUAD_STRIP";
		break;
	case GL_POLYGON:
		name = "GL_POLYGON";
		break;

	default: 
		name = TEXT("undefined enum: %d",cap);
	}
	return name;
}
void sys_glEnable (GLenum cap)
{
	if(logging){
		add_log("glEnable %s (%d)",GLenumToString(cap),cap);
	}

	(*orig_glEnable) (cap);
}

void sys_glPushMatrix (void)
{
	if(logging)
		add_log("glPushMatrix");
	(*orig_glPushMatrix) ();
}

void sys_wglSwapBuffers(HDC hDC)
{
	/* START MOVED */
	/* Aftermath: I moved all the stuff we need to do down to swapbuffers, as this is called
	 * when the application has finished its graphics process. This way, we only process once
	 * a frame. This will be much less CPU-intensive. */
	ExecuteCommands();

	if(GetAsyncKeyState(VK_END)&1){
		draw = !draw;
		draw_overlay = !draw_overlay;
	}

	/* Aftermath: I thought this was pretty cool :D */
	if(GetAsyncKeyState(VK_F1)&1) 
		draw_stride = 12;
	if(GetAsyncKeyState(VK_F2)&1)
		draw_stride = 16;
	if(GetAsyncKeyState(VK_F3)&1)
		draw_stride = 24;
	if(GetAsyncKeyState(VK_F4)&1)
		draw_stride = 20;
	if(GetAsyncKeyState(VK_F5)&1) //used in higher detail modes
		draw_stride = 36;
	if(GetAsyncKeyState(VK_F6)&1) // used in higher detail modes
		draw_stride = 40;
	if(GetAsyncKeyState(VK_F7)&1) // used in higher detail modes
		draw_stride = 28;
	if(GetAsyncKeyState(VK_F8)&1) // used in higher detail modes
		drawAll = !drawAll; //overlay for all models regardless of stride.
	if(GetAsyncKeyState(VK_PRIOR)&1){
		dataDisplay++;
		dataDisplay = dataDisplay % 4; //display options for the debugging. 
	}
	if(GetAsyncKeyState(VK_NEXT)&1)
		logging = !logging;
	/* END MOVED */	

	if(logging)
		add_log("wglSwapBuffers");

	
	EnterCriticalSection(&csCurrentModels);
	EnterCriticalSection(&csNewModels);

	/* START OVERLAY*/
	/* Aftermath: Moved overlay here so it is done only once per frame. MASSIVE SPEEDUP.
	 * I removed the disabling GL_SCISSORS_TEST since that causes glitchy graphics. */
	if(currentModels) {
		vector<Model *>::iterator it;
		for(it = currentModels->begin(); it != currentModels->end(); it++) {
			Model *drawModel = *it;
				
			if(draw_overlay && draw_stride == drawModel->stride || draw_overlay && drawAll) {
				orig_glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
				switch(dataDisplay){
					case 0:
					glPrint(drawModel->x_s,drawModel->y_s,"C: %u",drawModel->id);
					break;
					case 1:
					glPrint(drawModel->x_s,drawModel->y_s,"T: %u",drawModel->triangles);
					break;
					case 2:
					glPrint(drawModel->x_s,drawModel->y_s,"Xs: %d Ys: %d",drawModel->x_s,drawModel->y_s);
					break;
					case 3:
					glPrint(drawModel->x_s,drawModel->y_s,"X: %f Y: %f Z: %f",drawModel->x,drawModel->y,drawModel->z);
					break;
				}
			}
		}
	}
	/* END OVERLAY */

	/* Aftermath: Do this first in case something in the old models is used. [my knowledge/logic is questionable here]*/
	(*orig_wglSwapBuffers) (hDC);

	

	if(newModels) {
		if(currentModels) {
			vector<Model *>::iterator itOld = currentModels->begin();
			for(; itOld != currentModels->end(); itOld++) {
				delete *itOld;
			}
			delete currentModels;
		}

		currentModels = newModels;
		newModels = new vector<Model *>();

		/* Aftermath: we would be able to save a lot of memory if we stopped returning
		   values by reference; the current way, I can't delete models without being worried
		   that Simba is still using their functions, causing a crash. 
		   
		   Update: I've implemented a short term workaround to mitigate this - all models
		   returned to Simba are copied before being given to Simba, so that all other
		   models can be deleted. We should be mindful of memory - it's easy to run out of
		   space, causing Java to crash. */
	}
	LeaveCriticalSection(&csNewModels);
	LeaveCriticalSection(&csCurrentModels);	
}

void sys_BindTextureEXT(GLenum target, GLuint texture)
{
	if(logging)
		add_log("BindTextureEXT %d %d",target,texture);
	orig_BindTextureEXT(target,texture);
}

void sys_glAlphaFunc (GLenum func,  GLclampf ref)
{
	if(logging)
		add_log("AlphaFunc %d %f",func,ref);
	(*orig_glAlphaFunc) (func, ref);
}

void sys_glBegin (GLenum mode)
{
	if(logging){
		add_log("glBegin %s",GLenumToString(mode));
	}
	(*orig_glBegin) (mode);
}

void sys_glBitmap (GLsizei width,  GLsizei height,  GLfloat xorig,  GLfloat yorig,  GLfloat xmove,  GLfloat ymove,  const GLubyte *bitmap)
{
	if(logging)
		add_log("glBitmap"); // need to add vars was lazy;
	(*orig_glBitmap) (width, height, xorig, yorig, xmove, ymove, bitmap);
}

void sys_glBlendFunc (GLenum sfactor,  GLenum dfactor)
{
	if(logging)
		add_log("glBlendFunc"); // need to add vars was lazy;
	(*orig_glBlendFunc) (sfactor, dfactor);
}

void sys_glClear (GLbitfield mask)
{
	if(logging)
		add_log("glClear"); // need to add vars was lazy;
	(*orig_glClear)(mask);
}

void sys_glClearAccum (GLfloat red,  GLfloat green,  GLfloat blue,  GLfloat alpha)
{
	if(logging)
		add_log("glClearAccum"); // need to add vars was lazy;
	(*orig_glClearAccum) (red, green, blue, alpha);
}

void sys_glClearColor (GLclampf red,  GLclampf green,  GLclampf blue,  GLclampf alpha)
{
	if(logging)
		add_log("glClearColor"); // need to add vars was lazy;
	(*orig_glClearColor) (red, green, blue, alpha);
}

void sys_glColor3f (GLfloat red,  GLfloat green,  GLfloat blue)
{
	if(logging)
		add_log("glColor3f"); // need to add vars was lazy;
	(*orig_glColor3f) (red, green, blue);
}

void sys_glColor3ub (GLubyte red,  GLubyte green,  GLubyte blue)
{
	if(logging)
		add_log("glColor3ub"); // need to add vars was lazy;
	(*orig_glColor3ub) (red, green, blue);
}

void sys_glColor3ubv (const GLubyte *v)
{
	if(logging)
		add_log("glColor3ubv"); // need to add vars was lazy;
	(*orig_glColor3ubv) (v);
}

void sys_glColor4f (GLfloat red,  GLfloat green,  GLfloat blue,  GLfloat alpha)
{
	if(logging)
		add_log("glColor4f"); // need to add vars was lazy;
	(*orig_glColor4f) (red, green, blue, alpha);
}

void sys_glColor4ub (GLubyte red,  GLubyte green,  GLubyte blue,  GLubyte alpha)
{
	if(logging)
		add_log("glColor4ub"); // need to add vars was lazy;
	(*orig_glColor4ub) (red, green, blue, alpha);
}

void sys_glCullFace (GLenum mode)
{
	if(logging)
		add_log("glCullFace"); // need to add vars was lazy;
	(*orig_glCullFace) (mode);
}

void sys_glDeleteTextures (GLsizei n,  const GLuint *textures)
{
	if(logging)
		add_log("glDeleteTextures"); // need to add vars was lazy;
	(*orig_glDeleteTextures) (n, textures);
}

void sys_glDepthFunc (GLenum func)
{
	if(logging)
		add_log("glDepthFunc"); // need to add vars was lazy;
	(*orig_glDepthFunc) (func);
}

void sys_glDepthMask (GLboolean flag)
{
	if(logging)
		add_log("glDepthMask"); // need to add vars was lazy;
	(*orig_glDepthMask) (flag);
}

void sys_glDepthRange (GLclampd zNear,  GLclampd zFar)
{
	if(logging)
		add_log("glDepthRange"); // need to add vars was lazy;
	(*orig_glDepthRange) (zNear, zFar);
}

void sys_glDisable (GLenum cap)
{
	if(logging)
		add_log("glDisable %s",GLenumToString(cap)); // need to add vars was lazy;

	(*orig_glDisable) (cap);
}

void sys_glEnd (void)
{
	if(logging)
		add_log("glEnd");
	(*orig_glEnd) ();
}

void sys_glFrustum (GLdouble left,  GLdouble right,  GLdouble bottom,  GLdouble top,  GLdouble zNear,  GLdouble zFar)
{
	if(logging)
		add_log("glFrustum"); // need to add vars was lazy;
	(*orig_glFrustum) (left, right, bottom, top, zNear, zFar);
}

void sys_glPopName (void)
{
	if(logging)
		add_log("glPopName"); // need to add vars was lazy;
	(*orig_glPopName) ();
}

void sys_glPrioritizeTextures (GLsizei n,  const GLuint *textures,  const GLclampf *priorities)
{
	if(logging)
		add_log("glPrioritizeTextures "); // need to add vars was lazy;
	(*orig_glPrioritizeTextures) (n, textures, priorities);
}

void sys_glPushAttrib (GLbitfield mask)
{
	if(logging)
		add_log("glPushAttrib %d",mask); // need to add vars was lazy;
	(*orig_glPushAttrib) (mask);
}

void sys_glPushClientAttrib (GLbitfield mask)
{
	if(logging)
		add_log("glPushClientAttrib"); // need to add vars was lazy;
	(*orig_glPushClientAttrib) (mask);
}

void sys_glRotatef (GLfloat angle,  GLfloat x,  GLfloat y,  GLfloat z)
{
	if(logging)
		add_log("glRotatef %f %f %f %f",angle, x, y, z); // need to add vars was lazy;
	(*orig_glRotatef) (angle, x, y, z);
}

void sys_glShadeModel (GLenum mode)
{
	if(logging)
		add_log("glShadeModel"); // need to add vars was lazy;
	(*orig_glShadeModel) (mode);
}

void sys_glTexCoord2f (GLfloat s,  GLfloat t)
{
	if(logging)
		add_log("glTexCoord2f"); // need to add vars was lazy;
	(*orig_glTexCoord2f) (s, t);
}

void sys_glTexEnvf (GLenum target,  GLenum pname,  GLfloat param)
{
	if(logging)
		add_log("glTexEnvf"); // need to add vars was lazy;
	(*orig_glTexEnvf) (target, pname, param);
}

void sys_glTexImage2D (GLenum target,  GLint level,  GLint internalformat,  GLsizei width,  GLsizei height,  GLint border,  GLenum format,  GLenum type,  const GLvoid *pixels)
{
	if(logging){
		add_log("glTexImage2D %s %d %d %d %d %d %s %s ",GLenumToString(target),level,internalformat,width,height,border,GLenumToString(format),GLenumToString(type)); // need to add vars was lazy;
	}
	(*orig_glTexImage2D) (target, level, internalformat, width, height, border, format, type, pixels);
}

void sys_glTexParameterf (GLenum target,  GLenum pname,  GLfloat param)
{
	if(logging)
		add_log("glTexParameterf "); // need to add vars was lazy;
	(*orig_glTexParameterf) (target, pname, param);
}

void sys_glTranslated (GLdouble x,  GLdouble y,  GLdouble z)
{
	if(logging)
		add_log("glTranslated"); // need to add vars was lazy;
	(*orig_glTranslated) (x, y, z);
}

void sys_glTranslatef (GLfloat x,  GLfloat y,  GLfloat z)
{
	if(logging)
		add_log("glTranslatef "); // need to add vars was lazy;
	(*orig_glTranslatef) (x, y, z);
}

void sys_glVertex2f (GLfloat x,  GLfloat y)
{
	if(logging)
		add_log("glVertex2f %f %f",x,y); // need to add vars was lazy;
	(*orig_glVertex2f) (x, y);
}

void sys_glVertex3f (GLfloat x,  GLfloat y,  GLfloat z)
{
	if(logging)
		add_log("glVertex3f "); // need to add vars was lazy;
	(*orig_glVertex3f) (x, y, z);
}

void sys_glVertex3fv (const GLfloat *v)
{
	if(logging)
		add_log("glVertex3fv"); // need to add vars was lazy;
	(*orig_glVertex3fv) (v);
}

void sys_glViewport (GLint x,  GLint y,  GLsizei width,  GLsizei height)
{
	if(logging)
		add_log("glViewport %d %d %d %d",x, y, width, height); // need to add vars was lazy;
	(*orig_glViewport) (x, y, width, height);
}

PROC sys_wglGetProcAddress(LPCSTR ProcName)
{
	if (!strcmp(ProcName,"glMultiTexCoord2fARB"))
	{
		orig_glMultiTexCoord2fARB=(func_glMultiTexCoord2fARB_t)orig_wglGetProcAddress(ProcName);
		return (FARPROC)sys_glMultiTexCoord2fARB;
	}
	else if (!strcmp(ProcName,"glBindTextureEXT"))
	{
		orig_BindTextureEXT=(func_BindTextureEXT_t)orig_wglGetProcAddress(ProcName);
		return (FARPROC)sys_BindTextureEXT;
	}
	else if(!strcmp(ProcName,"glActiveTextureARB"))
	{
		orig_glActiveTextureARB=(func_glActiveTextureARB_t)orig_wglGetProcAddress(ProcName);
		return (FARPROC)sys_glActiveTextureARB;
	}
	else if(!strcmp(ProcName,"glBufferDataARB"))
	{
		orig_glBufferDataARB=(func_glBufferDataARB_t)orig_wglGetProcAddress(ProcName);
		return (FARPROC)sys_glBufferDataARB;
	}
	else if(!strcmp(ProcName,"glBindBufferARB"))
	{
		orig_glBindBufferARB=(func_glBindBufferARB_t)orig_wglGetProcAddress(ProcName);
		return (FARPROC)sys_glBindBufferARB;
	}

	return orig_wglGetProcAddress(ProcName);
}


#pragma warning(default:4100)

bool isLogging(){
	return logging;
}
void __cdecl add_log (const char * fmt, ...)
{
	va_list va_alist;
	char logbuf[256] = "";
	FILE * fp;

	va_start (va_alist, fmt);
	_vsnprintf (logbuf+strlen(logbuf), sizeof(logbuf) - strlen(logbuf), fmt, va_alist);
	va_end (va_alist);

	if ( (fp = fopen ("c:\\rs\\opengl32.log", "ab")) != NULL )
	{
		fprintf ( fp, "%s\n", logbuf );
		fclose (fp);
	}
}


#pragma warning(disable:4100)
BOOL __stdcall DllMain (HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpvReserved)
{
	switch(fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		InitializeCriticalSection(&csNewModels);
		InitializeCriticalSection(&csCurrentModels);
		EnterCriticalSection(&csNewModels);
		newModels = new vector<Model *>();
		LeaveCriticalSection(&csNewModels);

		DisableThreadLibraryCalls (hOriginalDll);
		CreateSharedMemory();
		return Init();

	case DLL_PROCESS_DETACH:
		if ( hOriginalDll != NULL )
		{
			FreeLibrary(hOriginalDll);
			hOriginalDll = NULL;
		}
		break;
	}
	return TRUE;
}
