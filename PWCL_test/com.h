#pragma once
#if defined(__AVR_ATmega168__) || defined(__AVR_ATmega328P__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
// Compile for Arduino
#define PRINT_SOURCE Serial.print("(A) ")
#define PRINT_MESSAGE(msg) Serial.print(msg)
#define PRINT_FLOAT(val) Serial.print(val, 5)
#else
// Compile for C++
#include <math.h> // for NAN
#include <string>
#include <string>
#include <iostream>
#define PRINT_SOURCE std::cout << "(C) "
#define PRINT_MESSAGE(msg) std::cout << msg
#define PRINT_FLOAT(val) printf("%.5f", val);
#endif
#define NUMVARS         16
#define BUFFERSIZE 500

class COM
{

public:

    void set(int index, float value)
    {
        arr[index] = value;
    }

    float get(int index)
    {
        return arr[index];
    }

    void printCurVals()
    {
        PRINT_MESSAGE("[");
        for (int i = 0; i < NUMVARS; i++) {
            PRINT_FLOAT(arr[i]);
            PRINT_MESSAGE(", ");
        }
        PRINT_MESSAGE("]\n");
    }

    // STRING StrCurVals()
    // {
    //     PRINT_MESSAGE("[");
    //     for (int i = 0; i < NUMVARS; i++) {
    //         PRINT_FLOAT(arr[i]);
    //         PRINT_MESSAGE(", ");
    //     }
    //     PRINT_MESSAGE("]\n");
    // }


    /***
      parses 'input' (which should be in an array format) for the parameters in the prespecified order
      Ex:
          if input = [23.89, 324]
          then
            arr[0] => 23.89
            arr[1] => 324
          then you could do..
            COM.get(0);
          which returns 23.89
    */
      bool deserialize_array(char* paramStr)
    {

    /* Ensure that the input string has the correct format and number of numbers to be parsed*/
    const char*  p = paramStr;
    unsigned int numB   = 0;   // number of brackets
    unsigned int numV   = 0;   // number of values

    while (*p) {
      if (*p == '[') { numB++;
      } else if ( *p == ']' ) {numB++;
      } else if ( *p == ',' ) {numV++;
      } p++;
    }
    if (numB != 2) {
        PRINT_SOURCE;
        PRINT_MESSAGE("Parse error, not valid array.. Message: \n");
        PRINT_MESSAGE(paramStr);
        PRINT_MESSAGE("\n");
        return false;
    }
    if (numV != NUMVARS) {
        PRINT_SOURCE;
        PRINT_MESSAGE("Parse error, incorrect array size.. Message: \n");
        PRINT_MESSAGE(paramStr);
        PRINT_MESSAGE("\n");
        return false;
    }

   char* pEnd;
   p = paramStr;
   for ( unsigned int i = 0; i < NUMVARS; i ++ ) {
      while(*p == '[' || *p == ' ') p ++ ;
      bool isNum = false;  // if the string is a number
      const char* nc = p; // nc will point to the next comma or the closing bracket
      /* Handle NANS */
       if ( (*p == 'N' && *(p+1) == 'A' && *(p+2) == 'N')
         || (*p == 'n' && *(p+1) == 'a' && *(p+2) == 'n') ) {
         //this->arr[i] != this->arr[i];  // TODO: this probably wont work #p1
         this->arr[i] = NAN;
         while( *p != ',' && p ) p ++; p ++; // goto past next comma
         continue;
       }
      while(*nc != ',' && *nc != ']' && *nc) {
        if ((int)*nc >= 48 && (int)*nc <= 57)
          isNum = true;
        nc++;
      }
      if (isNum) {
        this->arr[i] = strtod(p, &pEnd);
        p = pEnd;
      }
      while (*p != ',' && *p != ']' && *p) {
        p++;
      }
      p++;
   }
   p = paramStr;
   return true;
    }

private:
	float arr[NUMVARS];
};
