#include <thread_highways/dson/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <vector>

void empty_messages()
{
	hi::CoutScope scope("empty_messages");
	/*
	 * https://man7.org/linux/man-pages/man2/pipe.2.html
	 * The array pipefd is used to
	   return two file descriptors referring to the ends of the pipe.
	   pipefd[0] refers to the read end of the pipe.  pipefd[1] refers
	   to the write end of the pipe.
	*/
	int pipe_client_send_to_server[2];
	int pipe_server_send_to_client[2];
	if (pipe(pipe_client_send_to_server) == -1 || pipe(pipe_server_send_to_client))
	{
		scope.print("ERROR: failed to open pipe");
		return;
	}

	enum class Message : std::int32_t
	{
		IAmClient,
		IAmServer,
		AlarmEnemyDetected,
		AttackEnemy,
		EnemyDestroyed,
		GoShutdown,
		WorkFinished
	};

	const auto load_dson = [](const std::int32_t fd, hi::dson::DownloaderFromFD & downloder) -> bool
	{
		downloder.clear();
		hi::result_t result{hi::okInProcess};
		while (hi::okInProcess == result)
		{
			result = downloder.load(fd);
		}
		return (result > 0);
	};

	const auto server = [&]
	{
		hi::dson::DownloaderFromFD downloder;
		hi::dson::UploaderToFD uploader;

		const auto send = [&](Message message)
		{
			hi::dson::NewDson dson;
			dson.set_key_cast(message);
			uploader.upload(pipe_server_send_to_client[1], dson);
		};

		enum class State
		{
			WaitClientAuth,
			WaitEnemyDetection,
			WaitEnemyDestroyed,
			WaitClientShutdowned
		};
		// Гарант очерёдности сообщений
		State state_{State::WaitClientAuth};
		while (true)
		{
			if (!load_dson(pipe_client_send_to_server[0], downloder))
			{
				scope.print(std::string{"Server:FAIL: read_dson.load_from_fd, errno="}.append(std::to_string(errno)));
				continue;
			}
			auto dson = downloder.get_result();
			switch (dson->obj_key_cast<Message>())
			{
			case Message::IAmClient:
				{
					if (state_ != State::WaitClientAuth)
					{
						scope.print("Server:ERROR:state sequence is broken.");
						return;
					}
					scope.print("Server:received: Message::IAmClient");
					send(Message::IAmServer);
					state_ = State::WaitEnemyDetection;
				}
				break;
			case Message::AlarmEnemyDetected:
				{
					if (state_ != State::WaitEnemyDetection)
					{
						scope.print("Server:ERROR:state sequence is broken.");
						return;
					}
					scope.print("Server:received: Message::AlarmEnemyDetected");
					send(Message::AttackEnemy);
					state_ = State::WaitEnemyDestroyed;
				}
			case Message::EnemyDestroyed:
				{
					if (state_ != State::WaitEnemyDestroyed)
					{
						scope.print("Server:ERROR:state sequence is broken.");
						return;
					}
					scope.print("Server:received: Message::EnemyDestroyed");
					send(Message::GoShutdown);
					state_ = State::WaitClientShutdowned;
				}
			case Message::WorkFinished:
				{
					if (state_ != State::WaitClientShutdowned)
					{
						scope.print("Server:ERROR:state sequence is broken.");
						return;
					}
					scope.print("Server:received: Message::WorkFinished");
					return;
				}
				break;
			default:
				scope.print(std::string{"Server:UNKNOWN message:"}.append(std::to_string(dson->obj_key())));
				break;
			}
		}
		scope.print("Server:Finished");
	}; // server

	const auto client = [&]
	{
		hi::dson::DownloaderFromFD downloder;
		hi::dson::UploaderToFD uploader;

		const auto send = [&](Message message)
		{
			hi::dson::NewDson dson;
			dson.set_key_cast(message);
			uploader.upload(pipe_client_send_to_server[1], dson);
		};
		send(Message::IAmClient);

		enum class State
		{
			WaitServerAuth,
			WaitServerAtackOrder,
			WaitServerShutdownOrder,
		};
		// Гарант очерёдности сообщений
		State state_{State::WaitServerAuth};
		while (true)
		{
			if (!load_dson(pipe_server_send_to_client[0], downloder))
			{
				scope.print(std::string{"Client:FAIL: read_dson.load_from_fd, errno="}.append(std::to_string(errno)));
				continue;
			}
			auto dson = downloder.get_result();
			switch (dson->obj_key_cast<Message>())
			{
			case Message::IAmServer:
				{
					if (state_ != State::WaitServerAuth)
					{
						scope.print("Client:ERROR:state sequence is broken.");
						return;
					}
					scope.print("Client:received: Message::IAmServer");
					send(Message::AlarmEnemyDetected);
					state_ = State::WaitServerAtackOrder;
				}
				break;
			case Message::AttackEnemy:
				{
					if (state_ != State::WaitServerAtackOrder)
					{
						scope.print("Client:ERROR:state sequence is broken.");
						return;
					}
					scope.print("Client:received: Message::AttackEnemy");
					send(Message::EnemyDestroyed);
					state_ = State::WaitServerShutdownOrder;
				}
				break;
			case Message::GoShutdown:
				{
					if (state_ != State::WaitServerShutdownOrder)
					{
						scope.print("Client:ERROR:state sequence is broken.");
						return;
					}
					scope.print("Client:received: Message::GoShutdown");
					send(Message::WorkFinished);
					return;
				}
				break;
			default:
				scope.print(std::string{"Client:UNKNOWN message:"}.append(std::to_string(dson->obj_key())));
				break;
			}
		}
		scope.print("Client:Finished");
	}; // client

	std::thread server_thread(server);
	std::thread client_thread(client);

	client_thread.join();
	server_thread.join();

	close(pipe_client_send_to_server[0]);
	close(pipe_client_send_to_server[1]);
	close(pipe_server_send_to_client[0]);
	close(pipe_server_send_to_client[1]);
}

int main(int /* argc */, char ** /* argv */)
{
	empty_messages();

	std::cout << "Tests finished" << std::endl;
	return 0;
}
