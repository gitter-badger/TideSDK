
#include "posix_process.h"
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(OS_OSX)
# include <crt_externs.h>
# include <Poco/Path.h>
#endif

extern char **environ;
namespace ti
{	
	// Parts derived from Poco::Process_UNIX
	AutoPtr<PosixProcess> PosixProcess::currentProcess = new PosixProcess();

	/*static*/
	AutoPtr<PosixProcess> PosixProcess::GetCurrentProcess()
	{
		return currentProcess;
	}

	PosixProcess::PosixProcess() :
		logger(Logger::Get("Process.PosixProcess")),
		pid(-1),
		nativeIn(new PosixPipe(false)),
		nativeOut(new PosixPipe(true)),
		nativeErr(new PosixPipe(true))
	{
	}

	PosixProcess::PosixProcess() :
		Process(),
		logger(Logger::Get("Process.PosixProcess")),
		pid(getpid())
	{
		pid = getpid();
		for (int i = 0; environ[i] != NULL; i++)
		{
			std::string entry = environ[i];
			std::string key = entry.substr(0, entry.find("="));
			std::string val = entry.substr(entry.find("=")+1);
			
			SetEnvironment(key.c_str(), val.c_str());
		}
		
		SharedApplication app = Host::GetInstance()->GetApplication();
		for (size_t i = 0; i < app->arguments.size(); i++)
		{
			args->Append(Value::NewString(app->arguments.at(i)));
		}
	}

	PosixProcess::~PosixProcess()
	{
	}

	void PosixProcess::SetArguments(SharedKList args)
	{
#if defined(OS_OSX)
		std::string cmd = args->At(0)->ToString();
		size_t found = cmd.rfind(".app");
		if (found != std::string::npos)
		{
			Poco::Path p(cmd);
			std::string fn = p.getFileName();
			found = fn.find(".app");
			fn = fn.substr(0,found);
			fn = kroll::FileUtils::Join(cmd.c_str(),"Contents","MacOS",fn.c_str(),NULL);
			if (FileUtils::IsFile(fn))
			{
				cmd = fn;
			}
		}
		args->At(0)->SetString(cmd.c_str());
#endif
		Process:SetArguments(args);
	}

	void PosixProcess::ForkAndExec()
	{
		this->pid = fork();
		if (this->pid < 0)
		{
			throw ValueException::FromFormat("Cannot fork process for %s", 
				args->At(0)->ToString());
		}
		else if (pid == 0)
		{
			// outPipe and errPipe may be the same, so we dup first and close later
			dup2(nativeIn->GetReadHandle(), STDIN_FILENO);
			dup2(nativeOut->GetWriteHandle(), STDOUT_FILENO);
			dup2(nativeErr->GetWriteHandle(), STDERR_FILENO);
			nativeIn->Close();
			nativeOut->Close();
			nativeErr->Close();

			// close all open file descriptors other than stdin, stdout, stderr
			for (int i = 3; i < getdtablesize(); ++i)
				close(i);

			size_t i = 0;
			char** argv = new char*[args->Size() + 1];
			//argv[i++] = const_cast<char*>(command.c_str());
			for (;i < args->Size(); i++)
			{
				argv[i] = const_cast<char*>(args->At(i)->ToString());
			}
			argv[i] = NULL;

			SharedStringList envNames = environment->GetPropertyNames();
			for (i = 0; i < envNames->size(); i++)
			{
				const char* key = envNames->at(i)->c_str();
				std::string value = environment->Get(key)->ToString();
				setenv(key, value.c_str(), 1);
			}

			const char *command = args->At(0)->ToString();
			execvp(command, argv);
			_exit(72);
		}

		close(nativeIn->GetReadHandle());
		close(nativeOut->GetWriteHandle());
		close(nativeErr->GetWriteHandle());
	}

	void PosixProcess::MonitorAsync()
	{
		nativeOut->StartMonitor();
		nativeErr->StartMonitor();
	}

	std::string PosixProcess::MonitorSync()
	{
		SharedKMethod readCallback =
			StaticBoundMethod::FromMethod<PosixProcess>(
				this, &PosixProcess::ReadCallback);
		nativeOut->AddEventListener(Event::READ, readCallback);
		nativeErr->AddEventListener(Event::READ, readCallback);

		nativeOut->StartMonitor();
		nativeErr->StartMonitor();
		this->ExitMonitor();

		std::string output;
		for (size_t i = 0; i < processOutput.size(); i++)
		{
			output.append(processOutput.at(i)->Get());
		}

		return output;
	}

	int PosixProcess::Wait()
	{
		int status;
		int rc;
		do
		{
			rc = waitpid(this->pid, &status, 0);
		} while (rc < 0 && errno == EINTR);

		if (rc != this->pid)
		{
			throw ValueException::FromFormat("Cannot wait for process: %d", this->pid);
		}

		int exitCode = WEXITSTATUS(status);
		nativeOut->Close();
		nativeErr->Close();
		return exitCode;
	}

	void PosixProcess::ReadCallback(const ValueList& args, SharedValue result)
	{
		if (args.at(0)->IsObject())
		{
			SharedKObject data = args.GetObject(0)->GetObject("data");
			AutoBlob blob = data.cast<Blob>();
			if (!blob.isNull())
			{
				processOutput.push_back(blob);
			}
		}
	}
	
	int PosixProcess::GetPID()
	{
		return pid;
	}

	void PosixProcess::SendSignal(int signal)
	{
		if (kill(this->pid, signal) != 0)
		{
			switch (errno)
			{
			case ESRCH:
				throw ValueException::FromString("Couldn't find process");
			case EPERM:
				throw ValueException::FromString("Invalid permissions for terminating process");
			default:
				throw ValueException::FromFormat("Couldn't send signal: %d to process", signal);
			}
		}
	}

	void PosixProcess::Kill()
	{
		SendSignal(SIGKILL);
	}

	void PosixProcess::Terminate()
	{
		SendSignal(SIGINT);
	}

	bool PosixProcess::IsRunning()
	{
		return pid != -1;
	}

}
