
extern "C" {
#include "scope-checker.h"
#include <sys/apparmor.h>
#include <ubuntu-app-launch.h>
}

#include <unity-scopes.h>
#include "scope-checker-facade.h"

RuntimeFacade::RuntimeFacade () {
}

RuntimeFacade::~RuntimeFacade () {
}

class RuntimeReal : public RuntimeFacade {
public:
	unity::scopes::Runtime::UPtr runtime;
	RuntimeReal () {
		runtime = unity::scopes::Runtime::create();
	}

	unity::scopes::RegistryProxy registry () {
		return runtime->registry();
	}
};

ScopeChecker *
scope_checker_new ()
{
	auto fu_runtime = new RuntimeReal();
	return reinterpret_cast<ScopeChecker *>(fu_runtime);
}

void
scope_checker_delete (ScopeChecker * checker)
{
	auto runtime = reinterpret_cast<RuntimeFacade *>(checker);
	delete runtime;
}

int
scope_checker_is_scope (ScopeChecker * checker, const char * appid)
{
	if (checker == nullptr) {
		g_warning("%s:%d: Checker is '(null)'", __FILE__, __LINE__);
		return 0;
	}
	if (appid == nullptr) {
		g_warning("%s:%d: appid is '(null)'", __FILE__, __LINE__);
		return 0;
	}

	auto runtime = reinterpret_cast<RuntimeFacade *>(checker);

	try {
		auto registry = runtime->registry();
		registry->get_metadata(appid);
		return !0;
	} catch (unity::scopes::NotFoundException e) {
		return 0;
	} catch (...) {
		g_warning("Unable to read the Unity Scopes Registry");
		return 0;
	}
}

int
scope_checker_is_scope_pid (ScopeChecker * checker, pid_t pid)
{
	if (pid == 0)
		return 0;

	char * aa = nullptr;
	if (aa_gettaskcon(pid, &aa, nullptr) != 0) {
		return 0;
	}

	if (aa == nullptr)
		return 0;

	std::string appid(aa);
	free(aa);

	if (appid == "unconfined") {
		/* We're not going to support unconfined scopes, too hard */
		return 0;
	}

	gchar * pkg = nullptr;
	gchar * app = nullptr;
	if (ubuntu_app_launch_app_id_parse(aa, &pkg, &app, nullptr)) {
		appid= pkg;
		appid += "_";
		appid += app;

		g_free(pkg);
		g_free(app);
	}

	return scope_checker_is_scope(checker, appid.c_str());
}
