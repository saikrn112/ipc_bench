
#include "SignalHandler.h"

#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <iostream>
namespace qb
{

SignalHandler* SignalHandler::mInstance = NULL;
int SignalHandler::m_signal = -1;
bool SignalHandler::m_got_signal = false;

SignalHandler* SignalHandler::getInstance()
{
	if(!mInstance)
		mInstance = new SignalHandler;

	return mInstance;
}

SignalHandler::SignalHandler()
{
	setupHandlers();
}
SignalHandler::~SignalHandler()
{

}

/**
* Returns the bool flag indicating whether we received an exit signal
* @return Flag indicating shutdown of program
*/
bool SignalHandler::gotSignal()
{
	return m_got_signal;
}

void SignalHandler::setSignal(int signal)
{
	m_signal = signal;
}

int SignalHandler::getSignal()
{
	return m_signal;
}

/**
* Sets exit signal to true.
* @param[in] _ignored Not used but required by function prototype
*                     to match required handler.
*/
void SignalHandler::handle(int signal)
{
	m_signal = signal;
	m_got_signal = true;
}

/**
 * Resets the signal handler state after dealing with a given signal.
 */
void SignalHandler::reset()
{
	m_signal = -1;
	m_got_signal = false;
}

/**
* Set up the signal handlers for various exit signals
*/
void SignalHandler::setupHandlers()
{

	bool sigint = signal((int) SIGINT, SignalHandler::handle) != SIG_ERR;
	bool sigterm = signal((int) SIGTERM, SignalHandler::handle) != SIG_ERR;

	bool sigusr1 = signal((int) SIGUSR1, SignalHandler::handle) != SIG_ERR;
	bool sigusr2 = signal((int) SIGUSR2, SignalHandler::handle) != SIG_ERR;

	if(!(sigint && sigterm && sigusr1 && sigusr2))
	{
		throw std::runtime_error("Error setting up handlers...");
	}
}

void SignalHandler::setupCustomSignalHandler(int signal_type)
{
	bool is_handle_set = signal(signal_type, SignalHandler::handle) != SIG_ERR;

	if(!is_handle_set)
	{
		throw std::runtime_error("Error setting up custom handler for signal");
	}
}

bool SignalHandler::isExitSignal() const
{
	bool res = (this->getSignal() == SIGINT || this->getSignal() == SIGTERM);

	return res;
}

bool SignalHandler::isUserSignal() const
{
	bool res = (this->getSignal() == SIGUSR1 || this->getSignal() == SIGUSR2);

	return res;
}

bool SignalHandler::isSigpipeSignal() const
{
	bool res = (this->getSignal() == SIGPIPE);

	return res;
}

void wait()
{
	qb::SignalHandler* sh = qb::SignalHandler::getInstance();
	bool stop = false;

	while(!stop)
	{
		if(sh->gotSignal() && sh->isExitSignal())
		{
			stop = true;
		}

		sleep(1);
	}
}

}
