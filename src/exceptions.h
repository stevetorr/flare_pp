// Copied from Phoebe https://github.com/mir-group/phoebe, development branch, April 13, 2021

#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <cassert>
#include <iostream>
#include <string>

/** Object used to print an error message, and stop the code.
 */
class Error {
public:
  /** object constructor.
   * @param errorMessage: message to be print to the user.
   * @param errCode: return integer error code, should be different from 0!
   */
  explicit Error(const std::string &errMessage, const int &errCode = 1);
};

/** Object to print a warning to the user, without stopping the code
 */
class Warning {
public:
  explicit Warning(const std::string &errMessage);
};

#endif
