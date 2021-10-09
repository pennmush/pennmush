#include "config.h"

#include <ctype.h>
#include <string.h>
#include <math.h>
#ifdef I_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "copyrite.h"
#include "ansi.h"
#include "conf.h"
#include "externs.h"
#include "intrface.h"
#include "parse.h"
#include "game.h"
#include "match.h"
#include "confmagic.h"
#include "dbdefs.h"
#include "function.h"
#include "log.h"
#include "flags.h"
#include "attrib.h"
#include "mushdb.h"
#include "lock.h"
#include "strutil.h"

#include "tinyexpr.h"

FUNCTION(local_fun_math)
{
  if (!args[0] || !*args[0]) {
    safe_str("#-1 NO MATH EQUATION GIVEN!", buff, bp);
    return;
  }

  const char *c = args[0];
  int error;
  double r = te_interp(c, &error);

  if ( error != 0 ) {
    safe_format(buff, bp, "Error at character %d for expression %s.", error, args[0]);
    return;
  }

  safe_str(unparse_number(r), buff, bp);

  return;
}

void setupMathFunction() {
  function_add("MATH", local_fun_math, 1, 1, FN_REG);
}

int plugin_init() {
  setupMathFunction();
  return 1;
}

/**
int main(int argc, char *argv[])
{
    const char *c = "sqrt(5^2+7^2+11^2+(8-2)^2)";
    double r = te_interp(c, 0);
    printf("The expression:\n\t%s\nevaluates to:\n\t%f\n", c, r);
    return 0;
}
**/
