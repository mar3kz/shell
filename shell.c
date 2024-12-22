#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h> // systemove veci (read/write/close), procesy (fork/exec), slozky (chdir), F_OK...
#include <fcntl.h>  // systemove veci, procesy, uzivatele&skupiny (getuid), flags pro open()
#include <sys/types.h> // data type of password struct
#include <pwd.h>       // password struct
#include <sys/stat.h> // S_ISDIR(), stat struktura, stat
#include <dirent.h> // DIR
#include <ctype.h> // isdigit, isaplha, isupper, is...


// https://pubs.opengroup.org/onlinepubs/7908799/xsh/pwd.h.html
// https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/g/getpwnam.html
// https://stackoverflow.com/questions/2256945/removing-a-non-empty-directory-programmatically-in-c-or-c
// https://www.oreilly.com/library/view/c-in-a/0596006977/re194.html
// https://www.mkssoftware.com/docs/man5/struct_stat.5.asp
// https://pubs.opengroup.org/onlinepubs/009604599/functions/opendir.html

// pozor [x][x] => %c; [x] => %s
// vypsání velikosti memory page => getpagesize()
// flags = vlastnosti (v kontextu macros)
// stat struktura poskytuje detailni informace o souborech, funkce stat() ji naplni
// makro S_ISDIR() se pouziva na zjisteni, jestli je soubor slozka, bere jako argument cast struktury stat (st_mode) - flags (vlastnosti souboru)

// rm -s path => [0] = rm, [1] = -s, [2] = path
// cd path => [0] = cd, [1] = path, [2] = 0

// open, read, write je pro soubory, pro open je dulezite dat jako argumenty: (path, flags, execution permissions)
// opendir, closedir, readdir je pro slozky => potreba inicializace a deklarace pointeru na strukturu dirent (pointer, protoze je to vice efektivni)
// stream = koncept, ktery nam umoznuje praci s daty - cteni/zapisovani do ruznych zdroju
// access funkkce F_OK

/*
┌──(marek㉿kali)-[~/Documents]
└─$ cat /proc/1/stat
1 (systemd) S 0 1 1 0 -1 4194560 14032 157220 109 512 52 201 726 1865 20 0 1 0 61 23031808 3442 18446744073709551615 1 1 0 0 0 0 671173123 4096 1260 0 0 0 17 0 0 0 0 0 0 0 0 0 0 0 0 0 0
pid, nazev, stav, ..., tato hodnota znamena, jestli je ten proces pripojen k nejakemu terminalu
*/

// funkce fscanf vraci EOF pokud je error a pocet prvku, ktere potrebujeme, pokud je to spravne
// proces, ktery neni pripojen k zadnemu terminalu tam bude mit -1 a proces, ktery je pripojen k nejakemu terminalu tam bude mit cislo vetsi nez 0, 1 => /dev/tty1 = virtuální terminál 1, coz
// zname, ze pokud bychom chteli komunikovat s tim pocitacem jenom pomoci CLI, tak pro to jsou urceny ty terminaly
// funkce isaplha zjisti, jestli je x. prvek pismeno a funkce isdigit zjisti, jestli je x. prvek cislo

// ps -eo pid,tty,command

// funkce fscanf - vraci pocet uspesne nactenych polozek, neco se precte a kurzor zustane za tim prectenym prvkem

#define ROWS 3
#define COLUMNS 40

char **split(char *command);
char *input();
void free2D_arr(char **p_2D_arr);
int isDir(char *path);
int delete_filesDirectories(char **input);
int nestedFilesDeletion(char *nestedPath);
int my_mkdir(char **input);
int my_rmdir(char **input);
int ls(char **input);
int readingProcesses();
char nestedReadingStat(char *processes);
int isConnectedToTty(char *process);
char *uidToName(char *process);
void encrypt(int offset, char *toEncrypt);
void find(char *objectToFind, char *path);

int *num_chars;
char *officialPathForRm;
char lastUsedCommand[50];

int main(int argc, char *argv[])
{
   int RUID = getuid();
   char currentDir[50];
   char commandToExecute[50];

   struct passwd *p_pw;

   if ( (p_pw = getpwuid(RUID)) == NULL)
   {
       fprintf(stderr, "no user matching your RUID\n");
       exit(1);
   }
   printf("\n\n\n\n\n----(%s)-[%s]\n$ ", p_pw->pw_name, getcwd(currentDir, sizeof(currentDir)) );

   char *input_arr = input(); // 49 chars + \0, pokud vice nez 49 chars, tak zbytkove chars budou v stdin => nutny flush stdin bufferu

   char **array_2D = split(input_arr);



   // char *(*p_input)() = input; // function pointer, ktery ukazuje na funkci, ktera nema zadny parametr, input = pointer na funkci
   // char **(*p_split)(char *input_arr) = split; // function pointer, ktery ukazuje na funkci, ktera ma parametr char *input_arr, split = pointer na funkci

   while (1) // to exit = ctrl + C
   {
       if ( strcmp(array_2D[0], "cd") == 0)
       {
           snprintf(commandToExecute, sizeof(commandToExecute), "%s", array_2D[1]); // sizeof() - max delka toho commandu co se muze spustit + \0
           strcpy(lastUsedCommand, commandToExecute);
           //printf("%s", commandToExecute);
           if ( chdir(commandToExecute) == -1)
           {
               fprintf(stderr, "neco se pokazilo u cd");
           }
       }

       else if ( strcmp( array_2D[0], "rm") == 0)
       {
           officialPathForRm = array_2D[2];
           delete_filesDirectories(array_2D);
           //printf("\n%s", array_2D[1]);
           remove(array_2D[2]); // zkusi vymazat implicitne soubor v pwd
       }
       else if ( strcmp( array_2D[0], "pwd") == 0)
       {
           printf("\n%s\n", getcwd(currentDir, sizeof(currentDir)));
           strcpy(lastUsedCommand, "pwd");
       }
       else if ( strcmp( array_2D[0], "mkdir") == 0)
       {
           if ( my_mkdir(array_2D) != 0)
           {
               perror("error pri spousteni my_mkdir");
           }
           strcpy(lastUsedCommand, "mkdir");
           strcpy(lastUsedCommand, array_2D[1]);
       }
       else if ( strcmp ( array_2D[0], "rmdir") == 0)
       {
           if ( my_rmdir(array_2D) != 0)
           {
               perror("error pri spousteni my_rmdir");
           }
           strcpy(lastUsedCommand, "rmdir");
           strcpy(lastUsedCommand, array_2D[1]);
       }
       else if ( strcmp( array_2D[0], "ls") == 0)
       {
           if ( (ls(array_2D) != 0))
           {
               perror("error pri spousteni ls");
           }
       }
       else if ( strcmp ("exit", array_2D[0]) == 0)
       {
            return 1;
       }
       else if ( strcmp ("find", array_2D[0]) == 0)
       {
        find(array_2D[1], array_2D[2]);
       }

       printf("\n----(%s)-[%s]\n$ ", p_pw->pw_name, getcwd(currentDir, sizeof(currentDir)) );

       input_arr = input();
       array_2D = split(input_arr);
   }
  
   free(input_arr);
   free2D_arr(array_2D);
   }

char *input()
{
   /*
   tato funkce ma za ukol vzit useruv input do te doby nez stdin buffer naznaci EOF nebo novy radek
   */
   char c = '\n';
   start:
   while (c == '\n') // flushing stdin bufferu
   {
       c = getchar(); // bez goto by bylo infinite loop!!
       goto start;
   }

   char *input_arr = (char *)malloc(50);
   bzero(input_arr, 50);

   if (input_arr == NULL)
   {
       fprintf(stderr, "nedoslo k dynamickemu alokovani pameti");
       exit(1);
   }

   char character;
   int index = 1;

   input_arr[0] = c;
   while ( (character = getchar() ) != '\n' && character != EOF && index < 50) // po stisknuti enteru se bude brat po jednom charakteru z stdin ...
   {
       input_arr[index] = character;
       index++;
   }

   input_arr[index] = '\0';

   return input_arr;
}

char **split(char *command)
{
   /*
   tato funkce dynamicky alokuje 2D pole - rows, columns a take rozdeluje useruv input do tri casti [0], [1], [2]
   */
   int index = 0;

   char **p_Arr2D = (char **)malloc(ROWS * sizeof(char*)); // p ukazuje na memory, kde je prostor na 3 pointery, casting se dela vzdy podle promenne, * => *, ** => **, *** => ***...
   bzero(p_Arr2D, ROWS * sizeof(char));

   for (int index = 0; index < ROWS; index++)
   {
       p_Arr2D[index] = (char*)malloc(COLUMNS * sizeof(char));
       bzero(p_Arr2D[index], COLUMNS * sizeof(char));
   }


   // char *p_commandArr = (char *)malloc(50); // 1D pro nacteni charakteru ze scanf (argumentu)
   // for (int i = 0; i < strlen(command); i++)
   // {
   //     p_commandArr[i] = command[i];
   //     //printf("%c\n", command[index]);
   //     //command++; kdyby se ted chtelo command[1], tak by se vypsalo o, protoze pointer command ted ukazuje na h = 0. index, 1. index = o
   //     index = i;
   // }
   // p_commandArr[index] = '\0';

   int arr_i = 0, char_index = 0;

   for (int i = 0; i < strlen(command) && arr_i < ROWS; i++)
   {
       if (command[i] != ' ')
       {
           //printf("%c ", command[index]);
           p_Arr2D[arr_i][char_index] = command[i];
           char_index++;
           //printf("%c", p_Arr2D[arrayIndex][index]);
       }
       else
       {
           p_Arr2D[arr_i][char_index] = '\0';
           arr_i++;
           char_index = 0;
       } 
   }
   return p_Arr2D;
}

void free2D_arr(char **p_2D_arr)
{
   /*
   tato funkce ma starost freenout pamet od 2D dynamickeho pole (columns) + samotny pointer na pointer
   */
   for (int index = 0; index < ROWS; index++)
   {
       free(p_2D_arr[index]);
   }
   free(p_2D_arr); // potreba uvolnit i samotny ukazatel na uplny zacatek [0][0]
}

int delete_filesDirectories(char **input)
{
   /*
   tato funkce zjistuje, jestli user napsal jako argument ke commandu rm "-r" (recursive)/"-n" (normal), s tim ze "-n" je jen explicitni
   */
   char path[60];

   // zjisteni, jestli je input zadan jako rm -r path nebo rm path
   // rekurzivni mod, pro soubory a pro slozky
   if ( strcmp("-r", input[1]) == 0)
   {
       snprintf(path, sizeof(path), "%s", input[2]);

       // kontroluje, jestli ma proces pristup k urcitemu souboru/slozce na zaklade prav = F_OK = pokud existuje
       if ( access(path, F_OK) != 0)
       {
           fprintf(stderr, "\nneplatna cesta");
           exit(1);
       }
       strcpy(lastUsedCommand, input[0]);
       strcpy(lastUsedCommand, "-r");
       strcpy(lastUsedCommand, path);
       nestedFilesDeletion(path);       
   }

   // normalni mod, pro obycejne soubory, tato moznost se muze i nemusi pouzit, implicitne se zkusi vymazat aktualni zadana path v mainu
   else if ( strcmp("-n", input[1]) == 0)
   {
       snprintf(path, sizeof(path), "%s", input[2]);
       //printf("\nPATH: %s", path);

       if ( remove(path) != 0)
       {
           fprintf(stderr, "\nsoubor nejde vymazat");
           exit(1);
       }
       strcpy(lastUsedCommand, input[0]);
       strcpy(lastUsedCommand, "-n");
       strcpy(lastUsedCommand, path);
   }
}
// path se mysli path jenom slozka => /slozka = ANO, /slozka/soubor.txt = NE
int nestedFilesDeletion(char *path)
{
   /*
   tato funkce ma za ukol iterovat pres vsechny objekty na zadane ceste, pokud je objekt roven "."/".." => current directory/previous directory
   potom si zadanou cestu upravi na novou cestu (zadana_cesta/jmeno_objektu), potom se pokusi vymazat objekt na teto ceste (soubor/slozka),
   pokud vymazani selze, zjisti se, jestli je objekt slozka, pokud ano, tak se znovu upravi cesta na pristi (rekurzivni volani)
   */
   rmdir(path);

   DIR *dirStream = opendir(path); // reprezentuje stream, odkud se budou ukladat nazvy a informace o tich objektech
   struct dirent *currObject;

   if (dirStream == NULL)
   {
       // funkce uspela, udelala to, co ma odstranila path (officialPathForRm), toto muzeme pouzit, protoze uz kontrolujeme, jestli je zadana cesta validni
       return 0;
   }

   // pokud readdir udela to, co ma, vraci ukazatel, jinak NULL pointer
   while ( ( currObject = readdir(dirStream)) != NULL)
   {
       // printf("\nJMENO OBJEKTU: %s", currObject->d_name);  
       if ( strcmp(".", currObject->d_name) == 0 || strcmp("..", currObject->d_name) == 0)
       {
           // printf("COUNTER");
           // preskoci aktualni iteraci
           continue;
       }

       // currPath se mysli slozka i soubor => /slozka/soubor.txt = ANO, /slozka = NE
       char currPath[80];
       snprintf(currPath, sizeof(currPath), "%s/%s", path, currObject->d_name);

       // printf("\nObjekt v nestedFilesDeletion: %s, path: %s", currObject->d_name, currPath);

       if ( remove(currPath) != 0)
       {
           // printf("\nODSTRANEN: %s\n", currPath);
           //printf("\n%s", currObject->d_name);
           if ( isDir(currPath) == 0)
           {
               char nextPathForFiles[80];                 

               snprintf(nextPathForFiles, sizeof(nextPathForFiles), "%s", currPath);
               nestedFilesDeletion(nextPathForFiles);
           }
           printf("\n%s: soubor nejde odstranit\n", currPath);
       }
   }
   // printf("\nJDE SE TO SPUSTIT S OFICIALNI CESTOU");
   closedir(dirStream);
   nestedFilesDeletion(officialPathForRm);
}

int isDir(char *path)
{
   /*
   tato funkce zjisti, jestli je objekt slozka (file/dir) pomoci struktury stat, funkce stat a makra S_ISDIR
   return value: true = 0, false = 1
   */
   struct stat info;
   // printf("\nPath v isDir: %s\n", path);
   if ( stat(path, &info) != 0)
   {
       printf("\n\nTADY SE TO VYPISE: %s", path);
       perror("neco se pokazilo pri vyplnovani stat struktury v isDir");
       exit(1);
   }

   // S_ISDIR vraci true (nenulovou hodnotu) pri spravnem provedeni a 0 pri chybe
   if ( S_ISDIR(info.st_mode) == 1)
   {
       return 0;
   }
   return 1;
}

int my_mkdir(char **input)
{
   /*
   tato funkce slouzi bud na vytvoreni slozky na specificke ceste nebo v pwd
   */

   // -p = path, user uvede cestu a tam se vytvori nova slozka, pokud uz tam neexistuje
   // pozor na chybu permissioin denied => musim dat setuid bit, z RUID se stane EUID na root chmod u+s <soubor>
   if (strcmp( "-p", input[1]) == 0 && access(input[2], F_OK) != 0)
   {
       if (mkdir(input[2], 0777) != 0)
       {
           perror("nejde vytvorit novy adresar");
           return 1;
       }
       return 0;
   }

   // vytvareni nove slozky v pwd
   if (mkdir(input[1], 0777) != 0)
   {
       perror("\nnneco se pokazilo pri vytvareni nove slozky v pwd");
       return 1;
   }
   return 0;
}

int my_rmdir(char **input)
{
   /*
   tato funkce je velmi podobna mkdir, akorat misto vytvareni slozky se ta slozka odstrani (jen prazdna)
   */

   // odstranovani adresare na urcite adrese, pokud existuje
   if ( strcmp("-p", input[1]) == 0 && access(input[2], F_OK) == 0)
   {
       if (rmdir(input[1]) == 0)
       {
           perror("nepodarilo se ten adresar odstranit");
       }
       return 0;
   }

   // odstranovani adresare na pwd
   if (rmdir(input[1]) != 0 && access(input[1], F_OK) == 0)
   {
       perror("nepodarilo se ten adresar odstranit");
       return 1;
   }
   return 0;
}

int ls(char **input)
{
    /*
    tato funkce slouzi k vypsani vsech polozek v adresari na: -p (urcite ceste) - se skrytymi soubory/bez skrytych souboru/pwd
    */

    DIR *dirStream;
    struct dirent *currObject;
    // path, hidden > objekty
    if ( strcmp ( "-p-h", input[1]) == 0)
    {
        //printf("\npouzito -p-h\n");
        dirStream = opendir(input[2]);
        if (dirStream == NULL)
        {
            perror("neslo otevrit DIR stream v -p-h");
            return 1;
        }
        while ( (currObject = readdir(dirStream)) != NULL)
        {
            printf("\n%s \n", currObject->d_name);
        }
        strcpy(lastUsedCommand, "ls");
        strcpy(lastUsedCommand, "-p-h");
        strcpy(lastUsedCommand, input[2]);
    }
    // path > objekty
    else if (strcmp ( "-p", input[1]) == 0)
    {
        //printf("\npouzito -p\n");
        dirStream = opendir(input[2]);
        if (dirStream == NULL)
        {
            perror("neslo otevrit DIR stream v -p");
            return 1;
        }
        while ( ( currObject = readdir(dirStream)) != NULL)
        {
            if (strcmp ( ".", currObject->d_name) != 0 && strcmp( "..", currObject->d_name) != 0)
            {
                printf("\n%s ", currObject->d_name);
            }
            continue;            
        }
        strcpy(lastUsedCommand, "ls");
        strcpy(lastUsedCommand, "-p");
        strcpy(lastUsedCommand, input[2]);
    }
    else
    {
        // printf("\ntohle se udelalo");

        dirStream = opendir(".");
        
        if ( dirStream == NULL)
        {
            perror("neslo otevrit DIR stream v pwd");
            return 1;
        }

        while ( (currObject = readdir(dirStream)) != NULL)
        {
            if ( strcmp( ".", currObject->d_name) == 0 || strcmp( "..", currObject->d_name) == 0)
            {
                continue;
            }
            printf("\n%s ", currObject->d_name);
        }
        strcpy(lastUsedCommand, "ls");
        strcpy(lastUsedCommand, "xxx");
        strcpy(lastUsedCommand, input[2]);
    }
    return 0;
}

int readingProcesses()
{
    /*
    tato funkce slouzi k otevreni souboru /proc/stat, kde se nachazi informace o danem procesu jako napriklad, jestli je SLEEP, IDLE, ZOMBIE a take k opakovanemu volani funkce nestedReadingStat
    */
    DIR *dirStream = opendir("/proc");
    struct dirent *currObject;

    char path[40];

    if (dirStream == NULL)
    {
        perror("chyba pri otevirani slozky /proc v readingProcesses");
        exit(1);
    }
    //printf("\nOBJEKT v readingProcesses: %s\n", currObject->d_name);

    while ( ( currObject = readdir(dirStream)) != NULL)
    {
        if ( strcmp( ".", currObject->d_name) == 0 || strcmp( "..", currObject->d_name) == 0 || (isalpha(currObject->d_name[0]) != 0) ) // isaplha vraci 0, pokud to neni z abecedy
        {
            printf("\n%s", currObject->d_name);
            continue;
        }
       

        snprintf(path, sizeof(path), "/proc/%s", currObject->d_name);

        if (isDir(path) == 0)
        {
            snprintf(path, sizeof(path), "/proc/%s/stat", currObject->d_name);

            nestedReadingStat(currObject->d_name);
        }
    }

    closedir(dirStream);

    return 0;
}

char nestedReadingStat(char *processes)
{
    /*
    tato funkce slouzi k opakovanemu cteni souboru /proc/stat a zjistovani, jestli je dany proces SLEEPING/IDLE/ZOMBIE
    */
    char path[40];
    char objectInStat;

    snprintf(path, sizeof(path), "/proc/%s/stat", processes);
    printf("PATH v nestedReadingStat: %s", path);

    //int fileDescriptor = open(path, O_RDONLY);
    FILE *file_p = fopen(path, "r");

    if (file_p == NULL)
    {
        perror("neco se pokazilo pri otevirani souboru stat");
        exit(1);
    }

    // 3. vec => 2 index
    // tento for loop je protoze potrebujeme 8. prvek ve /proc/xx/stat => iteruji a nacitam postupne prvkz az se dostanu k 2. indexu (to, co potrebujeme)
    for (int index = 0; index < 3; index++)
    {
        if ( fscanf(file_p, "%c", objectInStat) != 1) // vraci pocet uspesne nactenych polozek - 1 prvek ("%s") => proto 1, fscanf ocekava file pointer jako prvni argument, kurzor zustava za koncem precteneho prvku
        {
            perror("neco se pokazilo pri nacitani prvku v nestedReadingStat");
        }
    }
    return objectInStat;
}

int isConnectedToTty(char *process)
{
    /*
    tato funkce slouzi k cteni vsech file descriptoru procesu, ktery jsme dostali jako argument, a zjistovani jestli je tento proces pripojen k nejakemu terminalu (tty),
    pokud ano, tak k jakemu terminalu
    proces pripojeny k terminalu (tty) = aspon jeden jeho file descriptor je pripojen k terminalu

    0 = ano, dany proces odkazuje na terminal
    1 = ne, dany proces neodkazuje na terminal
    */
    char path[40];
    char pathForFD[40];

    snprintf(path, sizeof(path), "/proc/%s/fd", process);
    
    DIR *dirStream = opendir(path);
    struct dirent *currObject;

    if (dirStream == NULL)
    {
        perror("neco se pokazilo pri otevirani slozky");
        exit(1);
    }

    while ( (currObject = readdir(dirStream) ) != NULL)
    {
        if ( strcmp( ".", currObject->d_name) == 0 || strcmp( "..", currObject->d_name) == 0)
        {
            continue;
        }

        snprintf(pathForFD, sizeof(pathForFD), "/proc/%s/fd/%s", process, currObject->d_name);
        int fileDescriptor = open(pathForFD, O_RDONLY);

        if (fileDescriptor == -1)
        {
            perror("neco se pokazilo s fileDescriptor");
        }

        printf("\nFile Descriptor: %d \t Current Object: %s", fileDescriptor, currObject->d_name);
        if ( fileDescriptor == -1)
        {
            perror("neco spatne s fileDescriptorem");
        }

        if ( isatty(fileDescriptor) == 1) // 1 = je terminal, 0 = neni terminal
        {
            printf("\nANO, JE\n");
            return 0;
        }
    }
    printf("\nne, neni");
    return 1;
}

char *uidToName(char *process) // char *
{
    /*
    tato funkce slouzi ke cteni souboru status daneho procesu a porovnani zacatek precteneho radky s "uid", pokud je shoda, vezmeme dane uid, pokud neni shoda cteme dal
    fgets pokud uspesne vraci pointer - naplneni bufferu, pokud selze (error, EOF) - vraci NULL
    fgets se bude snazit precist pocet zadanych Bytes, cteni prestane kdyz se narazi na \n nebo EOF nebo error
    */
    char path[40];
    snprintf(path, sizeof(path), "/proc/%s/status", process);
    //printf("%s", path);

    FILE *file_p = fopen(path, "r");
    char line[100];
    char *uid = malloc(sizeof(char) * 8);

    int uidIndex = 0;

    while ( ( fgets( line, sizeof(line), file_p)) != NULL) // pokusi se precist radek, pokud se to precte, tak se spusti ten while loop
    {
        char c;
        if ( line[0] == 'U' && line[1] == 'i') // Ui
        {
            for (int index = 0; index < strlen(line); index++)
            {
                if ( isdigit(line[index]) != 0) // pokud je to int, non-zero = int, jinak 0
                {
                    uid[uidIndex] = line[index];
                    uidIndex++;
                }
                // zjisteni jestli po precteni znaku/x. teho cisla, jestli je 0. index prazdny, coz by znamenalo, ze bychom jeste nezacali zapisovat do uid
                else if ( uidIndex > 0) // uid[0] != '\0' -> string neni prazdny - charakter je jiny od \0
                {
                    uid[uidIndex] = '\0';
                    //printf("\nUID: %s\n", uid);
                    return uid;
                }
                // else
                // {
                //     printf("neni to char");
                // }
            }
        }
    }
}

void encrypt(int offset, char *toEncrypt)
{
    /*
    tato funkce je Caesarova sifra, kde se koukam, jestli je "currObject" mezera, pokud ano, do bufferu pisu mezeru, pokud je "currObject" pres offset 122, tak chci vypocitat... (nakresli si to),
    stejne s tim offsetem u 90 tady jenom to musi platit pro velka pismena, jinak by to platilo i pro normalni, mala pismena a pokud je "currObject" pismeno, ktere je v rangy tich dvou bodu, tak
    se provede jenom ten char + offset
    */
    
    // <65 - 90> A, <97 - 122> a

    char encrypted[50] = {0}; // inicializuje vsechny Bytes na 0
    //printf("ENCRYPTED: %s, toEncrypt: %s\n", encrypted, toEncrypt);

    for (int index = 0; index < strlen(toEncrypt); index++)
    {
        if ( toEncrypt[index] == ' ')
        {
            encrypted[index] = ' ';
        }
        else if ( toEncrypt[index] + offset > 122)
        {
            int charsToLimit = 122 - toEncrypt[index];
            int charsToMoveFrom97 = offset - charsToLimit - 1; // charsToMoveFrom122 = offset - charsToLimit

            char newChar = 97 + charsToMoveFrom97;
            encrypted[index] = newChar;
        }
        else if ( toEncrypt[index] + offset > 90 && isupper( toEncrypt[index] ) )
        {
            int charsToLimit = 90 - toEncrypt[index];
            int charsToMoveFrom65 = offset - charsToLimit - 1; // charsToMoveFrom90 = offset - charsToLimit

            char newChar = 65 + charsToMoveFrom65;
            encrypted[index] = newChar;
        }
        else
        {
            char newChar = toEncrypt[index] + offset;
            encrypted[index] = newChar;
        }    
    }

    printf("%s", encrypted);
}

// a.txt /home/marek/Documents
void find(char *objectToFind, char *path)
{
    DIR *dirStream = opendir(path);
    struct dirent *currObject;

    if (dirStream == NULL)
    {
        perror("error pri otevirani cesty");
        exit(1);
    }

    while ( (currObject = readdir(dirStream)) != NULL)
    {
        if ( strcmp( ".", currObject->d_name) == 0 || strcmp( "..", currObject->d_name) == 0)
        {
            continue;
        }

        char currPath[50];
        snprintf(currPath, sizeof(currPath), "%s/%s", path, currObject->d_name);
        //snprintf(pathForFind, sizeof(pathForFind), "%s", currPath);

        if ( strcmp( objectToFind, currObject->d_name) == 0)
        {
            printf("\nPath, kde se to naslo: %s", currPath);
        }
        else if ( isDir(currPath) == 0)
        {
            find(objectToFind, currPath);
        }
    }
}