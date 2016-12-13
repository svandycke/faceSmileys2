/*!\file window.c
 *
 * \brief Utilisation de la SDL2/GL4Dummies et d'OpenGL 3+
 *
 * \author Far�s BELHADJ, amsi@ai.univ-paris8.fr
 * \modified by Steve VANDYCKE & Ali BAYRACK
 * \date April 24 2014 (modified December 12 2016)
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include <GL4D/gl4du.h>

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <objdetect.hpp>

using namespace cv;
using namespace std;
/*
 * Prototypes des fonctions statiques contenues dans ce fichier C
 */
static SDL_Window * initWindow(int w, int h, SDL_GLContext * poglContext);
static void initGL(SDL_Window * win);
static void initData(void);
static void resizeGL(SDL_Window * win);
static void loop(SDL_Window * win);
static void manageEvents(SDL_Window * win);
static void draw(void);
static void quit(void);

/*!\brief dimensions de la fen�tre */
static GLfloat _dim[] = {640, 480}; // 
/*!\brief pointeur vers la (future) fen�tre SDL */
static SDL_Window * _win = NULL;
/*!\brief pointeur vers le (futur) contexte OpenGL */
static SDL_GLContext _oglContext = NULL;
/*!\brief identifiant du (futur) vertex array object */
static GLuint _vao = 0;
/*!\brief identifiant du (futur) buffer de data */
static GLuint _buffer = 0;
/*!\brief identifiants des (futurs) GLSL programs */
static GLuint _pId = 0;
/*!\brief identifiant de la texture charg�e */
static GLuint _tId[2] = {0};
/*!\brief device de capture vid�o */
static VideoCapture * _cap = NULL;
/*!\brief Variable qui contient le fichier XML */
CascadeClassifier * face_cc = NULL;

/*!\brief La fonction principale initialise la biblioth�que SDL2,
 * demande la cr�ation de la fen�tre SDL et du contexte OpenGL par
 * l'appel � \ref initWindow, initialise OpenGL avec \ref initGL et
 * lance la boucle (infinie) principale.
 */
int main(int argc, char ** argv) {
  
  face_cc = new CascadeClassifier("haarcascade_frontalface_default.xml");

  if(SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "Erreur lors de l'initialisation de SDL :  %s", SDL_GetError());
    return -1;
  }
  atexit(SDL_Quit);
  if((_win = initWindow(_dim[0], _dim[1], &_oglContext))) {
    atexit(quit);
    gl4duInit(argc, argv);
    initGL(_win);
    _pId = gl4duCreateProgram("<vs>shaders/basic.vs", "<fs>shaders/basic.fs", NULL);
    initData();
    loop(_win);
  }
  return 0;
}

/*!\brief Cette fonction cr�� la fen�tre SDL de largeur \a w et de
 *  hauteur \a h, le contexte OpenGL \ref et stocke le pointeur dans
 *  poglContext. Elle retourne le pointeur vers la fen�tre SDL.
 *
 * Le contexte OpenGL cr�� est en version 3 pour
 * SDL_GL_CONTEXT_MAJOR_VERSION, en version 2 pour
 * SDL_GL_CONTEXT_MINOR_VERSION et en SDL_GL_CONTEXT_PROFILE_CORE
 * concernant le profile. Le double buffer est activ� et le buffer de
 * profondeur est en 24 bits.
 *
 * \param w la largeur de la fen�tre � cr�er.
 * \param h la hauteur de la fen�tre � cr�er.
 * \param poglContext le pointeur vers la case o� sera r�f�renc� le
 * contexte OpenGL cr��.
 * \return le pointeur vers la fen�tre SDL si tout se passe comme
 * pr�vu, NULL sinon.
 */
static SDL_Window * initWindow(int w, int h, SDL_GLContext * poglContext) {
  SDL_Window * win = NULL;
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  if( (win = SDL_CreateWindow("FaceSmiley", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
			      w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | 
			      SDL_WINDOW_SHOWN)) == NULL )
    return NULL;
  if( (*poglContext = SDL_GL_CreateContext(win)) == NULL ) {
    SDL_DestroyWindow(win);
    return NULL;
  }
  fprintf(stderr, "Version d'OpenGL : %s\n", glGetString(GL_VERSION));
  fprintf(stderr, "Version de shaders supportes : %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));  
  return win;
}

/*!\brief Cette fonction initialise les param�tres OpenGL.
 *
 * \param win le pointeur vers la fen�tre SDL pour laquelle nous avons
 * attach� le contexte OpenGL.
 */
static void initGL(SDL_Window * win) {
  glClearColor(0.2f, 0.2f, 0.2f, 0.0f);
  gl4duGenMatrix(GL_FLOAT, "projectionMatrix");
  gl4duGenMatrix(GL_FLOAT, "modelviewMatrix");
  gl4duBindMatrix("modelviewMatrix");
  gl4duLoadIdentityf();
  /* placer les objets en -10, soit bien apr�s le plan near (qui est � -2 voir resizeGL) */
  gl4duTranslatef(0, 0, -10);
  resizeGL(win);
}

/*!\brief Cette fonction param�trela vue (viewPort) OpenGL en fonction
 * des dimensions de la fen�tre SDL point�e par \a win.
 *
 * \param win le pointeur vers la fen�tre SDL pour laquelle nous avons
 * attach� le contexte OpenGL.
 */
static void resizeGL(SDL_Window * win) {
  int w, h;
  SDL_GetWindowSize(win, &w, &h);
  glViewport(0, 0, w, h);
  gl4duBindMatrix("projectionMatrix");
  gl4duLoadIdentityf();
  gl4duFrustumf(-1.0f, 1.0f, -h / (GLfloat)w, h / (GLfloat)w, 2.0f, 1000.0f);
}

static void initData(void) {
  GLfloat data[] = {
    /* 4 coordonn�es de sommets */
    -1.f, -1.f, 0.f, 1.f, -1.f, 0.f,
    -1.f,  1.f, 0.f, 1.f,  1.f, 0.f,
    /* 2 coordonn�es de texture par sommet */
    1.0f, 1.0f, 0.0f, 1.0f, 
    1.0f, 0.0f, 0.0f, 0.0f
  };
  glGenVertexArrays(1, &_vao);
  glBindVertexArray(_vao);

  glGenBuffers(1, &_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, _buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof data, data, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1); 
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const void *)0);  
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (const void *)((4 * 3) * sizeof *data));
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  glGenTextures(2, _tId);
  glBindTexture(GL_TEXTURE_2D, _tId[0]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindTexture(GL_TEXTURE_2D, _tId[1]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_NEAREST);
  
  Mat smiley;
  smiley = imread("mignonSmile.png", IMREAD_ANYCOLOR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, smiley.cols, smiley.rows, 0, GL_BGR /* GL_BGRA*/, GL_UNSIGNED_BYTE, smiley.data);

  _cap = new VideoCapture(0);
  if(!_cap || !_cap->isOpened()) {
    delete _cap;
    _cap = new VideoCapture(CV_CAP_ANY);
  }

  if(face_cc == NULL || !_cap->isOpened()) {
    if(face_cc) delete face_cc;
    if(_cap) delete _cap;
  }

  _cap->set(CV_CAP_PROP_FRAME_WIDTH,  (int)_dim[0]);
  _cap->set(CV_CAP_PROP_FRAME_HEIGHT, (int)_dim[1]);
}

/*!\brief Boucle infinie principale : g�re les �v�nements, dessine,
 * imprime le FPS et swap les buffers.
 *
 * \param win le pointeur vers la fen�tre SDL pour laquelle nous avons
 * attach� le contexte OpenGL.
 */
static void loop(SDL_Window * win) {
  SDL_GL_SetSwapInterval(1);
  for(;;) {
    manageEvents(win);
    draw();
    gl4duPrintFPS(stderr);
    SDL_GL_SwapWindow(win);
    gl4duUpdateShaders();
  }
}

/*!\brief Cette fonction permet de g�rer les �v�nements clavier et
 * souris via la biblioth�que SDL et pour la fen�tre point�e par \a
 * win.
 *
 * \param win le pointeur vers la fen�tre SDL pour laquelle nous avons
 * attach� le contexte OpenGL.
 */
static void manageEvents(SDL_Window * win) {
  SDL_Event event;
  while(SDL_PollEvent(&event)) 
    switch (event.type) {
    case SDL_KEYDOWN:
      switch(event.key.keysym.sym) {
      case SDLK_ESCAPE:
      case 'q':
	exit(0);
      default:
	fprintf(stderr, "La touche %s a ete pressee\n",
		SDL_GetKeyName(event.key.keysym.sym));
	break;
      }
      break;
    case SDL_KEYUP:
      break;
    case SDL_WINDOWEVENT:
      if(event.window.windowID == SDL_GetWindowID(win)) {
	switch (event.window.event)  {
	case SDL_WINDOWEVENT_RESIZED:
	  resizeGL(win);
	  break;
	case SDL_WINDOWEVENT_CLOSE:
	  event.type = SDL_QUIT;
	  SDL_PushEvent(&event);
	  break;
	}
      }
      break;
    case SDL_QUIT:
      exit(0);
    }
}

static void draw(void) {
  Mat ci, gsi;
  const GLfloat blanc[] = {1.0f, 1.0f, 1.0f, 1.0f};
  //const GLfloat bleu[]  = {0.5f, 0.5f, 1.0f, 1.0f};
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glUseProgram(_pId);
  glEnable(GL_DEPTH_TEST);

  //on commence a parameter et dessiner la capture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, _tId[0]);

  *_cap >> ci;
  vector<Rect> faces;
  cvtColor(ci, gsi, COLOR_BGR2GRAY);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ci.cols, ci.rows, 0, GL_BGR, GL_UNSIGNED_BYTE, ci.data); //charger la capture dans texture
  glUniform1i(glGetUniformLocation(_pId, "myTexture"), 0);
  glUniform1f(glGetUniformLocation(_pId, "width"), _dim[0]);
  glUniform1f(glGetUniformLocation(_pId, "height"), _dim[1]);
  
  /* Streaming au fond */
  gl4duBindMatrix("modelviewMatrix");
  gl4duPushMatrix(); /* sauver modelview */
  gl4duLoadIdentityf();
  gl4duTranslatef(0, 0, 0.9999f);
  gl4duBindMatrix("projectionMatrix");
  gl4duPushMatrix(); /* sauver projection */
  gl4duLoadIdentityf();
  gl4duSendMatrices(); /* envoyer les matrices */
  glUniform4fv(glGetUniformLocation(_pId, "couleur"), 1, blanc); /* envoyer une couleur */
  glBindVertexArray(_vao);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); /* dessiner le streaming (ortho et au fond) */
  gl4duPopMatrix(); /* restaurer projection */

  // Dessin du smiley
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, _tId[1]);
  glUniform1i(glGetUniformLocation(_pId, "myTexture"), 1);
  glUniform1f(glGetUniformLocation(_pId, "posx"), _dim[0]/* x */);
  glUniform1f(glGetUniformLocation(_pId, "posy"), _dim[1]/* y */);
  gl4duBindMatrix("modelviewMatrix");
  gl4duPopMatrix(); /* restaurer modelview */


  // D�tection du visage et affichage du/des smiley(s)
  face_cc->detectMultiScale(gsi, faces, 1.3, 5);
  for (vector<Rect>::iterator fc = faces.begin(); fc != faces.end(); ++fc) {

    /* Sauvegarde du modelview */
    gl4duPushMatrix();

    // Gestion du mouveament du smiley
    gl4duTranslatef(3.4-((((*fc).x)/_dim[0])*10), 3.4-((((*fc).y)/_dim[1])*10), 0);

    // Gestion de la taille du smiley 
    gl4duScalef((*fc).width/72, (*fc).height/72, 1);
        
    // Affichage du X et Y (Pour debugage)
    fprintf(stderr, "X = %d    Y = %d \n", (*fc).x, (*fc).y);

    /* Envoie des matrices */
    gl4duSendMatrices(); 
    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    /* Restauration du modelview */
    gl4duPopMatrix();
  }
}

/*!\brief Cette fonction est appel�e au moment de sortir du programme
 *  (atexit), elle lib�re la fen�tre SDL \ref _win et le contexte
 *  OpenGL \ref _oglContext.
 */
static void quit(void) {
  if(_cap) {
    delete _cap;
    _cap = NULL;
  }

  if(face_cc) {
    delete face_cc;
    face_cc = NULL;
  }


  if(_vao)
    glDeleteVertexArrays(1, &_vao);
  if(_buffer)
    glDeleteBuffers(1, &_buffer);
  if(_tId[0])
    glDeleteTextures(1, _tId);
  if(_oglContext)
    SDL_GL_DeleteContext(_oglContext);
  if(_win)
    SDL_DestroyWindow(_win);
  gl4duClean(GL4DU_ALL);
}
