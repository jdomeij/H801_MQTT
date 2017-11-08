





// Led gamma table
// https://learn.adafruit.com/led-tricks-gamma-correction/the-longer-fix
static const uint16_t s_gammaTable[] = {
     0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
     0,    0,    0,    0,    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
     1,    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
     1,    1,    1,    1,    1,    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
     2,    2,    2,    2,    2,    2,    2,    2,    2,    3,    3,    3,    3,    3,    3,    3,
     3,    3,    3,    3,    3,    3,    3,    3,    4,    4,    4,    4,    4,    4,    4,    4,
     4,    4,    4,    4,    4,    5,    5,    5,    5,    5,    5,    5,    5,    5,    5,    5,
     6,    6,    6,    6,    6,    6,    6,    6,    6,    6,    7,    7,    7,    7,    7,    7,
     7,    7,    7,    8,    8,    8,    8,    8,    8,    8,    8,    9,    9,    9,    9,    9,
     9,    9,   10,   10,   10,   10,   10,   10,   10,   11,   11,   11,   11,   11,   11,   11,
    12,   12,   12,   12,   12,   12,   13,   13,   13,   13,   13,   13,   14,   14,   14,   14,
    14,   14,   15,   15,   15,   15,   15,   16,   16,   16,   16,   16,   16,   17,   17,   17,
    17,   17,   18,   18,   18,   18,   18,   19,   19,   19,   19,   20,   20,   20,   20,   20,
    21,   21,   21,   21,   22,   22,   22,   22,   23,   23,   23,   23,   24,   24,   24,   24,
    24,   25,   25,   25,   26,   26,   26,   26,   27,   27,   27,   27,   28,   28,   28,   28,
    29,   29,   29,   30,   30,   30,   30,   31,   31,   31,   32,   32,   32,   33,   33,   33,
    33,   34,   34,   34,   35,   35,   35,   36,   36,   36,   37,   37,   37,   38,   38,   38,
    39,   39,   39,   40,   40,   40,   41,   41,   41,   42,   42,   42,   43,   43,   44,   44,
    44,   45,   45,   45,   46,   46,   47,   47,   47,   48,   48,   48,   49,   49,   50,   50,
    50,   51,   51,   52,   52,   52,   53,   53,   54,   54,   55,   55,   55,   56,   56,   57,
    57,   58,   58,   58,   59,   59,   60,   60,   61,   61,   62,   62,   62,   63,   63,   64,
    64,   65,   65,   66,   66,   67,   67,   68,   68,   69,   69,   70,   70,   71,   71,   72,
    72,   73,   73,   74,   74,   75,   75,   76,   76,   77,   77,   78,   78,   79,   79,   80,
    81,   81,   82,   82,   83,   83,   84,   84,   85,   85,   86,   87,   87,   88,   88,   89,
    89,   90,   91,   91,   92,   92,   93,   94,   94,   95,   95,   96,   97,   97,   98,   98,
    99,  100,  100,  101,  102,  102,  103,  103,  104,  105,  105,  106,  107,  107,  108,  109,
   109,  110,  111,  111,  112,  113,  113,  114,  115,  115,  116,  117,  117,  118,  119,  119,
   120,  121,  122,  122,  123,  124,  124,  125,  126,  127,  127,  128,  129,  130,  130,  131,
   132,  132,  133,  134,  135,  135,  136,  137,  138,  139,  139,  140,  141,  142,  142,  143,
   144,  145,  146,  146,  147,  148,  149,  150,  150,  151,  152,  153,  154,  154,  155,  156,
   157,  158,  159,  159,  160,  161,  162,  163,  164,  165,  165,  166,  167,  168,  169,  170,
   171,  171,  172,  173,  174,  175,  176,  177,  178,  179,  180,  180,  181,  182,  183,  184,
   185,  186,  187,  188,  189,  190,  191,  192,  193,  193,  194,  195,  196,  197,  198,  199,
   200,  201,  202,  203,  204,  205,  206,  207,  208,  209,  210,  211,  212,  213,  214,  215,
   216,  217,  218,  219,  220,  221,  222,  223,  224,  226,  227,  228,  229,  230,  231,  232,
   233,  234,  235,  236,  237,  238,  239,  241,  242,  243,  244,  245,  246,  247,  248,  249,
   251,  252,  253,  254,  255,  256,  257,  258,  260,  261,  262,  263,  264,  265,  267,  268,
   269,  270,  271,  272,  274,  275,  276,  277,  278,  280,  281,  282,  283,  285,  286,  287,
   288,  289,  291,  292,  293,  294,  296,  297,  298,  299,  301,  302,  303,  304,  306,  307,
   308,  310,  311,  312,  313,  315,  316,  317,  319,  320,  321,  323,  324,  325,  327,  328,
   329,  331,  332,  333,  335,  336,  337,  339,  340,  342,  343,  344,  346,  347,  348,  350,
   351,  353,  354,  355,  357,  358,  360,  361,  362,  364,  365,  367,  368,  370,  371,  373,
   374,  375,  377,  378,  380,  381,  383,  384,  386,  387,  389,  390,  392,  393,  395,  396,
   398,  399,  401,  402,  404,  405,  407,  408,  410,  412,  413,  415,  416,  418,  419,  421,
   422,  424,  426,  427,  429,  430,  432,  434,  435,  437,  438,  440,  442,  443,  445,  446,
   448,  450,  451,  453,  455,  456,  458,  460,  461,  463,  465,  466,  468,  470,  471,  473,
   475,  476,  478,  480,  482,  483,  485,  487,  488,  490,  492,  494,  495,  497,  499,  501,
   502,  504,  506,  508,  509,  511,  513,  515,  517,  518,  520,  522,  524,  526,  527,  529,
   531,  533,  535,  536,  538,  540,  542,  544,  546,  548,  549,  551,  553,  555,  557,  559,
   561,  563,  564,  566,  568,  570,  572,  574,  576,  578,  580,  582,  584,  586,  587,  589,
   591,  593,  595,  597,  599,  601,  603,  605,  607,  609,  611,  613,  615,  617,  619,  621,
   623,  625,  627,  629,  631,  633,  635,  637,  639,  641,  644,  646,  648,  650,  652,  654,
   656,  658,  660,  662,  664,  666,  669,  671,  673,  675,  677,  679,  681,  683,  686,  688,
   690,  692,  694,  696,  699,  701,  703,  705,  707,  710,  712,  714,  716,  718,  721,  723,
   725,  727,  729,  732,  734,  736,  738,  741,  743,  745,  747,  750,  752,  754,  757,  759,
   761,  763,  766,  768,  770,  773,  775,  777,  780,  782,  784,  787,  789,  791,  794,  796,
   798,  801,  803,  806,  808,  810,  813,  815,  818,  820,  822,  825,  827,  830,  832,  835,
   837,  839,  842,  844,  847,  849,  852,  854,  857,  859,  862,  864,  867,  869,  872,  874,
   877,  879,  882,  884,  887,  889,  892,  894,  897,  899,  902,  905,  907,  910,  912,  915,
   917,  920,  923,  925,  928,  930,  933,  936,  938,  941,  944,  946,  949,  952,  954,  957,
   959,  962,  965,  968,  970,  973,  976,  978,  981,  984,  986,  989,  992,  995,  997, 1000
 };



/**
 * H801 Led
 */
class H801_Led {
private:
  String   m_id;
  uint8_t  m_pwm_index;
  int      m_bri;

  double    m_fadeBri;
  double    m_fadeStep;
  uint32_t  m_fadeNum;

public:
  H801_Led(String id) {
    m_id = id;
    m_pwm_index = 0;
    m_bri = 0;

    m_fadeBri = 0;
    m_fadeStep = 0;
    m_fadeNum = 0;

  }

  
  void setup(uint8_t pin, uint8_t pwm_index) {
    pinMode(pin, OUTPUT);
    m_pwm_index = pwm_index;
  }


  /**
   * Get ID for pin
   * @return ID
   */
  String& get_ID() {
    return m_id;
  }


  /**
   * Return current brightness
   * @return brightness
   */
  uint8_t get_Bri() {
    return m_bri>>2; 
  }


  /**
   * Return current brightness
   * @return brightness
   */
  uint16_t get_RawBri() {
    return m_bri; 
  }


  /**
   * Set brightness using JsonVariant
   * @param  item      JSON object value
   * @param  fadeSteps Number of steps to fade over
   * @return Was the led value changed
   */
  bool set_Bri(JsonVariant &item, uint32_t fadeSteps) {
    // Number
    if (item.is<int>()) {
      return this->set_Bri((uint8_t)constrain(item.as<int>(), 0x00, 0xFF), fadeSteps);
    }

    // string
    if (item.is<char*>()) {
      unsigned long tmp = 0;
      // Parse string
      if (!stringToUnsignedLong(item.as<char*>(), &tmp))
        return false;

      return this->set_Bri((uint8_t)constrain(tmp, 0x00, 0xFF), fadeSteps);
    }

    // Unhandled value type
    return false;
  }

  /**
   * Set brightness for led
   * @param  bri       New brightness
   * @param  fadeSteps Number of steps to fade over
   * @return Was the led value changed
   */
  bool set_Bri(uint8_t bri, uint32_t fadeSteps) {
    // Clear any current fading
    m_fadeNum = 0;

    // No change
    if (m_bri>>2 == bri)
      return false;

    // No duration update pwm directly
    if (!fadeSteps) {
      m_bri = (bri<<2 | 0x3) & 0x3FF;
      pwm_set_duty(s_gammaTable[m_bri], m_pwm_index);
      return true;
    }


    // Calculate step size and set fading flag
    // Set the bri to the target value so the change event contains the target values
    int fadeDiff = (int16_t)(((bri<<2) - m_bri) | 0x3);
    m_fadeStep = (double)(fadeDiff)/(double)(fadeSteps);
    m_fadeBri = m_bri;
    m_fadeNum = fadeSteps + 1;

    // Set current bri to target so get_Bri returns correct value, 
    // this value will be overwritten on first do_Fade before updating pin
    m_bri = (bri<<2 | 0x3) & 0x3FF;

    return true;
  }

  /**
   * Fade led value one step
   * @return Are we still fading
   */
  bool do_Fade() {
    // Fading light?
    if (!m_fadeNum)
      return false;

    // Decrease fading counter
    m_fadeNum--;

    // Fade one step
    m_fadeBri += m_fadeStep;
    m_fadeBri = constrain(m_fadeBri, 0x0, 0x3FF);

    // Convert to 10-bit number
    m_bri = ((int)(m_fadeBri)) & 0x3FF;

    // Update light
    pwm_set_duty(s_gammaTable[m_bri], m_pwm_index);
    //analogWrite(m_pin, s_gammaTable[m_bri]);
    
    // Last fade event, clear values
    if (!m_fadeNum) {
      m_fadeBri = 0;
      m_fadeStep = 0;
      return false;
    }

    // Still fading
    return true;
  }

  /**
   * Overrides current fade and fades single step,
   * @param dirUp Are we fading toward max
   */
  void do_ButtonFade(bool dirUp) {
    m_fadeBri = 0;
    m_fadeStep = 0;
    m_fadeNum = 0;

    if (dirUp && m_bri < 0x3FF)
      m_bri += 5;
    else if (!dirUp && m_bri > 0)
      m_bri -= 5;

    m_bri = constrain(m_bri, 0x0, 0x3FF);

    // Update light
    pwm_set_duty(s_gammaTable[m_bri], m_pwm_index);
  }
};

