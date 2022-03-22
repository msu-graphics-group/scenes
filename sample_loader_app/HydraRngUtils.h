namespace hr_prng
{
  struct uint2
  {
    unsigned int x;
    unsigned int y;
  };

  typedef struct RandomGenT
  {
    uint2 state;

  } RandomGen;

  RandomGen RandomGenInit(const int a_seed);

  /**
   \brief get next pseudo random float value in range [0,1].
   \param gen - pointer to current generator state
   */
  float rndFloat(RandomGen *gen);

  /**
   \brief get next pseudo random float value in range [s,e].
   \param gen - reference to current generator state
   \param s   - low  boundary of generated random number
   \param e   - high boundary of generated random number
   */
  float rndFloatUniform(RandomGen& gen, float s, float e);

  /**
   \brief get next pseudo random integer value in range [s,e].
   \param gen - reference to current generator state
   \param s   - low  boundary of generated random number
   \param e   - high boundary of generated random number
   */
  int rndIntUniform(RandomGen& gen, int a, int b);
};

namespace hr_qmc
{
  constexpr static int   QRNG_DIMENSIONS = 11;
  constexpr static int   QRNG_RESOLUTION = 31;
  constexpr static float INT_SCALE       = (1.0f / (float)0x80000001U);

  void init(unsigned int table[hr_qmc::QRNG_DIMENSIONS][hr_qmc::QRNG_RESOLUTION]);

  /**
  \brief get arbitrary 'Sobol/Niederreiter' quasi random float value in range [0,1]
  \param pos     - id of value in sequence (i.e. first, second, ... )
  \param dim     - dimention/coordinate id (i.e. x,y,z,w, ... )
  \param c_Table - some table previously initialised with hr_qmc::init function

  */
  float rndFloat(unsigned int pos, int dim, unsigned int *c_Table); ///< return

  /**
  \brief get arbitrary 'Sobol/Niederreiter' quasi random float value in range [s,e].
  \param gen - pointer to current generator state
  \param s   - low  boundary of generated random number
  \param e   - high boundary of generated random number
  */
  float rndFloatUniform(unsigned int pos, int dim, unsigned int *c_Table, float s, float e);

  /**
  \brief get arbitrary 'Sobol/Niederreiter' quasi random integer value in range [s,e].
  \param gen     - pointer to current generator state
  \param s   - low  boundary of generated random number
  \param e   - high boundary of generated random number
  */
  int rndIntUniform(unsigned int pos, int dim, unsigned int *c_Table, int a, int b);
};

