/*!
 * \file   notacomponent.cpp
 * \brief  A valid shared library that is deliberately NOT a HydroCouple
 *         component, used to prove the loader rejects such files with a
 *         diagnostic instead of crashing.
 *
 * This is the ordinary case in a real plugin directory: dependencies sit
 * beside components, and the loader must walk past them calmly.
 */

#if defined(_WIN32)
#  define NOTACOMPONENT_EXPORT __declspec(dllexport)
#else
#  define NOTACOMPONENT_EXPORT __attribute__((visibility("default")))
#endif

extern "C" NOTACOMPONENT_EXPORT int notacomponent_answer(void)
{
  return 42;
}
