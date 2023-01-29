/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_MAILBOXES_IMAIL_BOX_SEND_HERE_H
#define THREADS_HIGHWAYS_MAILBOXES_IMAIL_BOX_SEND_HERE_H

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

#endif // THREADS_HIGHWAYS_MAILBOXES_IMAIL_BOX_SEND_HERE_H
