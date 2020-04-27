
#include <Arduino.h>


// typedef enum {

// } name_of_variable;

class Utils {

  public:
    static void init(bool debugMode=false);
    static void test();

  private:
    Utils() {} // Prevent creating instances of this class
    static bool isDebugging;

  protected:

};
