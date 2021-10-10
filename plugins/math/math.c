#include "tinyexpr.h"

#include "config.h"

#include "conf.h"
#include "externs.h"
#include "parse.h"
#include "function.h"
#include "strutil.h"

FUNCTION(local_fun_tinyexpr)
{
  char *c;
  int error;
  double r;

  if (!args[0] || !*args[0]) {
    safe_str("#-1 NO MATH EXPRESSION GIVEN!", buff, bp);
    return;
  }

  c = args[0];
  r = te_interp(c, &error);

  if (error != 0) {
    safe_format(buff, bp, "Error at character %d for expression %s.", error, c);
    return;
  }

  safe_str(unparse_number(r), buff, bp);

  return;
}

void setupMathFunction() {
  function_add("TINYEXPR", local_fun_tinyexpr, 1, 1, FN_REG | FN_STRIPANSI | FN_NOPARSE);
}

int plugin_init() {
  setupMathFunction();
  return 1;
}
