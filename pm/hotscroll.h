struct KEYOPT
{
   int scroll_lock : 2;

   int normal : 1; /* tells if normal and/or timer scrolling is active */
   int timer  : 1;

   int button_1 : 1;
   int button_2 : 1;
   int button_3 : 1;
   int alt : 1;
   int ctrl : 1;
   int shift : 1;
   int custom_key_toggle : 1;
   char custom_key;
   char character;
   USHORT virtualkey;

   int button_12 : 1;
   int button_22 : 1;
   int button_32 : 1;
   int alt2 : 1;
   int ctrl2 : 1;
   int shift2 : 1;
   int custom_key_toggle2 : 1;
   char custom_key2;
   char character2;
   USHORT virtualkey2;
};

#define DEFAULT_KEYOPT {1,1,1,0,0,1,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0}

struct APPOPT
{
   USHORT speed;
   USHORT posperpix;

   ULONG msrange;
   float slope;
   ULONG mspix;
   USHORT rangefraction;
   USHORT pospertimeout;
   ULONG pixmin;
   ULONG pixmax;


   int speed_toggle : 2;
   int nonprop : 2;

   int enabled : 2;
   int vertical : 2;
   int horizontal : 2;

   int mouse_leash : 2;
   int no_focus_lock : 2;
   int fake_dynamic : 2;
   int reversed : 2;


   int timerdefault: 1;

   int linear_inverse : 1; /* 1 = linear, 0 = inverse */
   int timer_nonprop : 1;
};

#define DEFAULT_APPOPT {100,0,450,2.0,10000,50,5,10,200,1,0,1,1,1,0,0,0,0,1,1,0}
#define INIT_APPOPT    {100,0,450,2.0,10000,50,5,10,200,2,2,2,2,2,2,2,2,2,1,1,0}
