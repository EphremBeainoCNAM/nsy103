#define ALTMAX 20000
#define ALTMIN 1

#define VITMAX 1000
#define VITMIN 200

#define PAUSE 2

struct coordonnees {
  int x;
  int y;
  int altitude;
};

struct deplacement {
  int cap;
  int vitesse;
};

struct avion {
    struct coordonnees position;
    struct deplacement dep;
};
