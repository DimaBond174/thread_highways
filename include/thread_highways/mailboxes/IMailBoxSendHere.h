#ifndef IMAILBOX_H
#define IMAILBOX_H

#include <cstdint>

namespace hi
{

template <typename T>
struct IMailBoxSendHere
{
	virtual ~IMailBoxSendHere() = default;

	// Multi-threaded access
	virtual bool send_may_fail(T && t) = 0;
	// Если вернул false, значит почтового ящика больше нет и слать снова незачем
	virtual bool send_may_blocked(T && t) = 0;
};

} // namespace hi

#endif // IMAILBOX_H
