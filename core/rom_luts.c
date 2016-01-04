struct game_type
{
   char product_number[11];
   int dynarec_type;        /* -1, make no decision, 0 = dynamic recompiler, 1 = generic_recompiler */
   int aica_interrupt_hack; /* -1, make no decision, 0 = normal, 1 = enable hack */
   f32 zMin;
   f32 zMax;
};

static struct game_type lut_games[] = 
{
   { "MK-51049  ",  1, -1, 0, 1 },                /* Marvel Vs Capcom 2 */
   { "T1203N    ", -1,  1, 0, 1 },                /* Street Fighter Alpha 3 */
   { "MK0815    ", -1, -1, 0, 3000.0 },           /* Soul Calibur (E) */
   { "T1401M    ", -1, -1, 0, 3000.0 },           /* Soul Calibur (J) */
};
