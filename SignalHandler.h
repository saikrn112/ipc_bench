#ifndef SIGNALHANDLER_H_
#define SIGNALHANDLER_H_

namespace qb
{
	/**
	 * Base class for signal handlers of various kinds.
	 */
	class SignalHandler
	{
	public:
		static SignalHandler* getInstance();
		virtual ~SignalHandler();

		static bool gotSignal();
		static void setSignal(int signal);
		static int getSignal();

		static void handle(int signal);
		static void reset();

		bool isExitSignal() const;
		bool isUserSignal() const;
		bool isSigpipeSignal() const;

		void setupCustomSignalHandler(int signal_type);

	protected:
		SignalHandler();
		virtual void setupHandlers();

	private:
		static SignalHandler* mInstance;
		static bool m_got_signal;
		static int m_signal;

	};

	/**
	 * function that can block the main() of an app and catches kill signals, will respond to User signals as well
	 */
	void wait();
}

#endif /* EXITSIGNALHANDLER_H_ */
