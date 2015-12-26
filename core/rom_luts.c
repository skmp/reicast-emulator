struct game_type
{
   char product_number[11];
   int dynarec_type; /* -1, make no decision, 0 = dynamic recompiler, 1 = generic_recompiler */
};

static struct game_type lut_games[] = 
{
   { "MK-51049  ", 1 }                 /* Marvel Vs Capcom 2 (JUE) */
};
