
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xatom.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <math.h>

//create a shared memory ID
int shmid = 0; //initiailize to 0
int ipckey;

//----------------------
typedef double Myfloat;
//----------------------

struct Global {
	Display *dpy;
	Window win;
	GC gc;
	int xres, yres;
    unsigned int foreground_color;

    Atom wm_delete_window;

    //----------------
    Myfloat box[4][2];
    //----------------
} g;

struct Box {
    int x, y;
    int h, w;
} box;

struct Shared {
    char *ptt;
    char sender;
    char *msg;
    int x, y, inside, close, stop, rot_speed;
    unsigned int box_color;
} *shared;

int check_x_click(XEvent *e);

void x11_cleanup_xwindows(void);
void x11_init_xwindows(void);
void x11_clear_window(void);
void check_mouse(XEvent *e);
int check_keys(XEvent *e);
void render(void);
int get_argument_count(char**);

int child = 0;

int count = 0;

char **myargv;
char **myenvp;

int fill = 0;
int done = 0;

// reroutes render to main from thread
int render_flag = 0;

//speedometer for rotating box
//shared->rot_speed = 30000; //default


void *myThread(void *arg)
{
    //XInitThreads();
    //
    while(!done) {
    
        if (child) {
            // If parent right clicks inside box
            // Then child window closes
            if(shared->close == 1){
                done = 1;
                // shared->close back to 0
                shared->close = 0;
            }
            // If parent moves mouse inside box
            // Then child box fills
            // note: Must call render to fill box w/o moving to child window
            // note: When calling render, child screen does not appear.
            // This is why render_flag is needed, so render can be called
            // form main instead.
            if(shared->inside == 1) {
                // set child box to mirror parent box when holding left click
                box.x = shared->x;
                box.y = shared->y;
                fill = 1;
                //render();
                render_flag = 1;
            }
            else {
                fill = 0;
                //render();
                render_flag = 1;
            }
        }
    }

    return (void *)0;
}

int main(int argc, char *argv[], char *envp[])
{
    argc = get_argument_count(argv);

    char pathname[128];

    myargv = argv;
    myenvp = envp;

    int timer = 0;

    int *status;
    pthread_t tid;

    //set argc to 0

    if (argc > 1 && strcmp(argv[1], "child") == 0) {
        if (strcmp(argv[1], "child") == 0){
            //shared->stop = 0;
            child = 1;
            //x11_init_xwindows();
            //get the shmid from argv[2] using atoi()
            shmid = atoi(argv[2]); //child reads shared memory ID from argv[2] sent from parent
            printf("child shmid: %i\n", shmid);
            //then...
            
            shared = shmat(shmid, (void *) 0, 0);
            shared->ptt = "c";
            shared->msg = "start";
            shared->rot_speed = 30000;
            //shared->stop = 0;
            //start the thread...
            //int *status;
            //pthread_t tid;
            pthread_create(&tid, NULL, myThread, (void *)0);
            //pthread_join(tid, (void **)&status); //needed type cast to (void **)
        }
    }
    else {
        
        
        //--------------------
        //setup box in parent
        g.box[0][0] = -51.0;
        g.box[1][0] = -51.0;
        g.box[2][0] =  51.0;
        g.box[3][0] =  51.0;

        g.box[0][1] = -51.0;
        g.box[1][1] =  51.0;
        g.box[2][1] =  51.0;
        g.box[3][1] = -51.0;
        //--------------------

        //create ipckey
        ipckey = ftok(pathname, 21);
        //parent sets shared memory ID
        shmid = shmget(ipckey, sizeof(int), IPC_CREAT | 0666);
        printf("parent shmid: %i\n", shmid);
        //attach to shared memory
        shared = shmat(shmid, (void *)0, 0);
        shared->ptt = "c";
        shared->stop = 0;
    }

    XEvent e;
    //change color of child window compared to parent window
    if(child == 1) {
        g.foreground_color = 0x00ffbb00;
    }
    else {
        g.foreground_color = 0x00ff0000;
    }
	//int done = 0;
	x11_init_xwindows();
    box.x = g.xres/2;
    box.y = g.yres/2;
    box.w = box.h = 40;

    // initialize shared->x and shared->y 
    // to box.x and box.y so mirroring looks natural
    shared->x = box.x;
    shared->y = box.y;


    
	while (!done) {
        
	    /* Check the event queue */
	    while (XPending(g.dpy)) {
	        XNextEvent(g.dpy, &e);
	        check_mouse(&e);
            //--------------------------------------------------------
            //usleep(1000);//REMOVED -> was causing flickering
            //--------------------------------------------------------

		    //done = check_keys(&e);
            done |= check_keys(&e);
            done |= check_x_click(&e);
		    render();
            //render_flag = 1;
		}
		usleep(4000);
        if(argc > 1 && atoi(argv[1]) > 0) {
            timer++;
            if(timer == atoi(argv[1]))
            done = 1;
        }
        //-----------------------------------------
        if (!child && shared->stop == 1) {
            //printf("here\n"); fflush(stdout);
            render_flag = 1;
            //rot_speed determines the speed of rotation
            usleep(shared->rot_speed);
        }
        //-----------------------------------------
        
        // Render flag
        // When needing to render inside of thread
        // thread calls render flag
        // then render flag calls render() from main
        if (render_flag != 0) {
            render();
            render_flag = 0;
        }
	}
    
	x11_cleanup_xwindows();
    shmctl(shmid, IPC_RMID, 0);

    if (child) {
        pthread_join(tid, (void **)&status); //needed type cast to (void **)
    }

	return 0;
}

int get_argument_count(char **argv)
{
    int i = 0;
    //int i = 1;
    //while(argv[++i] != NULL);
    while(*argv++ != NULL) {
        i++;
    };
    printf("argc: %i\n", i);

    return i;
}

int check_x_click(XEvent *e)
{
    
    //Code donated courtesy of Taylor Hooser Spring 2023.
    if (e->type != ClientMessage)
        return 0;
    //if x button clicked, DO NOT CLOSE WINDOW
    //instead, overwrite default handler
    if ((Atom)e->xclient.data.l[0] == g.wm_delete_window)
        return 1;
    

    return 0;
}


void x11_cleanup_xwindows(void)
{
	XDestroyWindow(g.dpy, g.win);
	XCloseDisplay(g.dpy);
}

void x11_init_xwindows(void)
{
	int scr;

	if (!(g.dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "ERROR: could not open display!\n");
		exit(EXIT_FAILURE);
	}
	scr = DefaultScreen(g.dpy);
    //x = 400, y = 200
	g.xres = 500;
	g.yres = 250;
	g.win = XCreateSimpleWindow(g.dpy, RootWindow(g.dpy, scr), 1, 1,
							g.xres, g.yres, 0, 0x00ffffff, 0x00000000);
	XStoreName(g.dpy, g.win, "cs3600 xwin sample");
	g.gc = XCreateGC(g.dpy, g.win, 0, NULL);
	XMapWindow(g.dpy, g.win);
	XSelectInput(g.dpy, g.win, ExposureMask | StructureNotifyMask |
								PointerMotionMask | ButtonPressMask |
								ButtonReleaseMask | KeyPressMask);
    g.wm_delete_window = XInternAtom(g.dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g.dpy, g.win, &g.wm_delete_window, 1);
}

void check_mouse(XEvent *e)
{
    static int offsetx, offsety;
    static int lbuttondown = 0;
    static int inside = 0;
    static int count = 0;
	static int savex = 0;
	static int savey = 0;
	int mx = e->xbutton.x;
	int my = e->xbutton.y;

	if (e->type != ButtonPress
		&& e->type != ButtonRelease
		&& e->type != MotionNotify)
		return;
    if (e->type == ButtonRelease) {
        lbuttondown = 0;   
    }
	if (e->type == ButtonPress) {
		if (e->xbutton.button==1) {
            if (inside) {
                lbuttondown = 1;
                offsetx = mx - box.x;
                offsety = my - box.y;
            }
            //--------------------------------------------
            //on "click" in child window
            if (my > 60 && my < 70){
                if (mx > 10 && mx < 40) {
                    if (child) {
                        //stop box rotation in parent
                        if (shared->stop == 0) {
                            printf("Stop\n");
                            shared->stop = 1;
                            shared->msg = "stop";
                            render_flag = 1;
                        }
                        //resume box rotation in parent
                        else if (shared->stop == 1) {
                            printf("Resume\n");
                            shared->stop = 0;
                            shared->msg = "start";
                            render_flag = 1;
                        }
                    }

                }
            }
            //--------------------------------------------
        }
		if (e->xbutton.button==3) {
            if (inside) {
                // Closes child window if 
                // right click inside parent box
                if (!child) {
                    shared->close = 1;
                    return;
                }
            }
        }
	}
	if (e->type == MotionNotify) {
		if (savex != mx || savey != my) {
			/*mouse moved*/
			savex = mx;
			savey = my;
            if (++count > 20) {
                if (child)
                    printf("c");
                else
                    printf("p");
                fflush(stdout);
                count = 0;
            }
            if (lbuttondown) {
                box.x = mx - offsetx;
                box.y = my - offsety;
                // If moving parent box
                // also move child box
                if (!child) {
                    shared->x = box.x;
                    shared->y = box.y;
                }
                return;
            }

            inside = 0;
            if (mx > box.x && mx < box.x+box.w) {
                if (my > box.y && my < box.y+box.h) {
                    if(++count > 20) {
                        printf("b");
                        fflush(stdout);
                        count = 0;
                    }
                    inside = 1;
                }
            }
            //-----------------------------------------------
            //setup speed bar located across top of parent
            //using the last line as the speedometer
            //----------------------------------------------- CURRENT
            if (child){  
                if (my > 60 && my < 75) {
                    if (mx > 20 && mx < 30) {
                        //set speed to slowest
                        shared->rot_speed = 60000;
                        //printf("here1\n");
                    }
                    if (mx >= 30 && mx < 50) {
                        //set speed to slow-med
                        shared->rot_speed = 50000;
                        //printf("here2\n");
                    }
                    if (mx >= 70 && mx < 100) {
                        //set speed to medium
                        shared->rot_speed = 40000;
                        //printf("here3\n");
                    }
                    if (mx >= 100 && mx < 120) {
                        //set speed to med-fast
                        shared->rot_speed = 35000;
                        //printf("here4\n");
                    }
                    if (mx >= 120 && mx < 150) {
                        //set speed to fastest
                        shared->rot_speed = 30000;
                        //printf("here5\n");
                    }
                    if (mx > 150 && mx < 170) {
                        //set speed to slowest
                        shared->rot_speed = 25000;
                        //printf("here6\n");
                    }
                    if (mx >= 170 && mx < 200) {
                        //set speed to slow-med
                        shared->rot_speed = 20000;
                        //printf("here7\n");
                    }
                    if (mx >= 200 && mx < 250) {
                        //set speed to medium
                        shared->rot_speed = 15000;
                        //printf("here8\n");
                    }
                    if (mx >= 250 && mx < 280) {
                        //set speed to med-fast
                        shared->rot_speed = 10000;
                        //printf("here9\n");
                    }
                    if (mx >= 280 && mx < 310) {
                        //set speed to fastest
                        shared->rot_speed = 5000;
                        //printf("here10\n");
                    }
                    if (mx >= 310 && mx < 340) {
                        shared->rot_speed = 2000;
                        //printf("here11\n");
                    }
                } 
            }

            if (inside && lbuttondown) {
                box.x = mx;
                box.y = my;
            }
		}
       
        // If in parent box, fill child box 
        if (inside) {
            if (!child) {
                shared->inside = 1;
                return;
            }
        }
        else {
            if (!child) {
                shared->inside = 0;
                return;
            }
        }
        //----------------------------------
	}
}

void create_child()
{
   //pipe(xChild);
   pid_t pid = fork();
   if (pid == 0) {
        child = 1;
        count = count + 1;
        char str[16]; //char string to hold shared memory id
        sprintf(str, "%d", shmid); //read schmid from parent into str
        //char *arr[3] = {myargv[0], "child", NULL};
        //send str holding parent's schmid into child's argument
        char *arr[4] = {myargv[0], "child", str, NULL};
        execve(myargv[0], arr, myenvp); //calls program again with different arguments
   }
 
}

void moveWindow(int rx, int ry)
{
    const int titleBarHeight = 23;
    //Get current position
    XWindowAttributes xwa;
    Window child;
    Window root = DefaultRootWindow(g.dpy);
    int x, y;
    XTranslateCoordinates(g.dpy, g.win, root, 0, 0, &x, &y, &child);
    XGetWindowAttributes(g.dpy, g.win, &xwa);
    XMoveWindow(g.dpy, g.win, x + rx, y + ry - titleBarHeight);

}

int check_keys(XEvent *e)
{
	int key;
	if (e->type != KeyPress && e->type != KeyRelease)
		return 0;
	key = XLookupKeysym(&e->xkey, 0);
	if (e->type == KeyPress) {
		switch (key) {
            case XK_Right:
                //Right arrow was pressed
                if (!child)
                    moveWindow(10, 0);
                break;
            case XK_Left:
                //Left arrow was pressed
                if(!child)
                    moveWindow(-10, 0);
                break;
            case XK_Up:
                //Up arrow was pressed
                if(!child)
                    moveWindow(0, -10);
                break;
            case XK_Down:
                //Down arrow was pressed
                if(!child)
                    moveWindow(0, 10);
                break;
			case XK_c:
                create_child();
				break;
            case XK_g:
                g.foreground_color = 0x00ffbb33;
                break;
            case XK_b:
                g.foreground_color = 0x002233ff;
               break; 
			case XK_Escape:
				return 1;
		}
	}
	return 0;
}

void drawText(int x, int y, char *str)
{
    XDrawString(g.dpy, g.win, g.gc, x, y, str, strlen(str));
}

void drawLine(int x1, int y1, int x2, int y2)
{
    XDrawLine(g.dpy, g.win, g.gc, x1, y1, x2, y2);
}

void render(void)
{
    //function drawText is a function in the code 
    //above the render function
    if (child) {
        XInitThreads();
        //set Background color to yellow
        XSetForeground(g.dpy, g.gc, 0x00ffbb55);
        XFillRectangle(g.dpy, g.win, g.gc, 0, 0, g.xres, g.yres);
        XFlush(g.dpy); //added
        //set text color to black
        //text spaced by 14
        XSetForeground(g.dpy, g.gc, 0x00000000);
        drawText(10, 16, "This is Project Phase-1");
        drawText(10, 30, "Child window");
        drawText(10, 54, "Drag box with left mouse button.");
        drawText(10, 68, "Click to ");
        drawText(65, 68, shared->msg);
        drawText(105, 68, "[][][][][][][][][][][][][][][][][][][][][][][]");
        //drawText(10, 74, "shmid: ");
        //drawText(58, 74, myargv[2]);

        //set color for box
        XSetForeground(g.dpy, g.gc, 0x00000000);
        //draw box
        //added to fill box in child while mouse is inside parent box
        if (fill == 1) {
            XFillRectangle(g.dpy, g.win, g.gc, box.x, box.y, box.w, box.h);
        }
        if (fill == 0) {
            XDrawRectangle(g.dpy, g.win, g.gc, box.x, box.y, box.w, box.h);
        }
    }
    else {
        //set background color to red
        XSetForeground(g.dpy, g.gc, g.foreground_color);
        XFillRectangle(g.dpy, g.win, g.gc, 0, 0, g.xres, g.yres);
        XFlush(g.dpy); //added
        //set text color to white
        //text spaced by 14
        XSetForeground(g.dpy, g.gc, 0x00ffffff);
        drawText(10, 16, "This is Project Phase-1");
        drawText(10, 30, "Parent window");
        drawText(10, 54, "Drag box with left mouse button.");
        //x = 10, y = 68
        drawText(10, 68, "Right-click inside box to close the child.");
        //set color for box
        XSetForeground(g.dpy, g.gc, 0x00ffffff);
        //draw box
        XDrawRectangle(g.dpy, g.win, g.gc, box.x, box.y, box.w, box.h);
        
        //---------------------------------------------------------------
        //draw box in parent window
        //drawLine(10, 10, 100, 100); //REMOVED
        int x = g.xres/6;  //set x and y to reposition box
        int y = g.yres/1.5;
        static Myfloat angle = 0.0;
        Myfloat mat[2][2] = {{cos(angle), -sin(angle)},
                         {sin(angle), cos(angle)}};
        int i;
        Myfloat b[4][2];
        memcpy(b, g.box, sizeof(Myfloat)*4*2);
        for (i = 0; i<4; i++) {
            b[i][0] = g.box[i][0]*mat[0][0] + g.box[i][1]*mat[0][1];
            b[i][1] = g.box[i][0]*mat[1][0] + g.box[i][1]*mat[1][1];
        }
        for (i = 0; i < 4; i++) {
            int j = (i + 1) % 4;
            //rotate
            drawLine(b[i][0]+x, b[i][1]+y, b[j][0]+x, b[j][1]+y);
        }
        // only spin the box if the render_flag is 1
        if (render_flag == 1) {
            angle += 0.1; // spins the box
        }
        //---------------------------------------------------------------
        
    }
}

