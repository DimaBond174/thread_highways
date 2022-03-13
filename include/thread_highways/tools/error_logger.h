#ifndef ERROR_LOGGER_H
#define ERROR_LOGGER_H

#include <functional>
#include <memory>
#include <string>

namespace hi
{

/*
Потоко безопасный sink для записи/отправки информации об ошибках
*/
using ErrorLogger = std::function<void(std::string)>;
using ErrorLoggerPtr = std::shared_ptr<ErrorLogger>;

} // namespace hi

#endif // ERROR_LOGGER_H
