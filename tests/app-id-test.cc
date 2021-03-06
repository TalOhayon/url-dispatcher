/**
 * Copyright (C) 2013 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "test-config.h"

#include <gio/gio.h>
#include <gtest/gtest.h>
#include "dispatcher.h"
#include "ubuntu-app-launch-mock.h"
#include "overlay-tracker-mock.h"

class AppIdTest : public ::testing::Test
{
	private:
		GTestDBus * testbus = nullptr;
		GMainLoop * mainloop = nullptr;
		gchar * cachedir = nullptr;
		OverlayTrackerMock tracker;

	protected:
		virtual void SetUp() {
			g_setenv("TEST_CLICK_DB", "click-db", TRUE);
			g_setenv("TEST_CLICK_USER", "test-user", TRUE);

			cachedir = g_build_filename(CMAKE_BINARY_DIR, "app-id-test-cache", nullptr);
			g_setenv("URL_DISPATCHER_CACHE_DIR", cachedir, TRUE);

			testbus = g_test_dbus_new(G_TEST_DBUS_NONE);
			g_test_dbus_up(testbus);

			mainloop = g_main_loop_new(nullptr, FALSE);
			dispatcher_init(mainloop, reinterpret_cast<OverlayTracker *>(&tracker));

			return;
		}

		virtual void TearDown() {
			dispatcher_shutdown();

			/* Clean up queued events */
			while (g_main_pending()) {
				g_main_iteration(TRUE);
			}

			g_main_loop_unref(mainloop);

			ubuntu_app_launch_mock_clear_last_app_id();

			g_test_dbus_down(testbus);
			g_object_unref(testbus);

			gchar * cmdline = g_strdup_printf("rm -rf \"%s\"", cachedir);
			g_spawn_command_line_sync(cmdline, nullptr, nullptr, nullptr, nullptr);
			g_free(cmdline);
			g_free(cachedir);
			return;
		}
};

TEST_F(AppIdTest, BaseUrl)
{
	gchar * out_appid = nullptr;
	const gchar * out_url = nullptr;

	/* Good sanity check */
	dispatcher_url_to_appid("appid://com.test.good/app1/1.2.3", &out_appid, &out_url);
	ASSERT_STREQ("com.test.good_app1_1.2.3", out_appid);
	EXPECT_EQ(nullptr, out_url);

	dispatcher_send_to_app(out_appid, out_url);
	EXPECT_STREQ("com.test.good_app1_1.2.3", ubuntu_app_launch_mock_get_last_app_id());

	ubuntu_app_launch_mock_clear_last_app_id();
	g_clear_pointer(&out_appid, g_free);
	out_url = nullptr;

	/* No version at all */
	dispatcher_url_to_appid("appid://com.test.good/app1", &out_appid, &out_url);

	EXPECT_EQ(nullptr, out_appid);
	EXPECT_EQ(nullptr, out_url);

	ubuntu_app_launch_mock_clear_last_app_id();
}

TEST_F(AppIdTest, WildcardUrl)
{
	gchar * out_appid = nullptr;
	const gchar * out_url = nullptr;

	/* Version wildcard */
	dispatcher_url_to_appid("appid://com.test.good/app1/current-user-version", &out_appid, &out_url);
	ASSERT_STREQ("com.test.good_app1_1.2.3", out_appid);
	EXPECT_EQ(nullptr, out_url);

	dispatcher_send_to_app(out_appid, out_url);
	EXPECT_STREQ("com.test.good_app1_1.2.3", ubuntu_app_launch_mock_get_last_app_id());

	ubuntu_app_launch_mock_clear_last_app_id();
	g_clear_pointer(&out_appid, g_free);
	out_url = nullptr;

	/* First app */
	dispatcher_url_to_appid("appid://com.test.good/first-listed-app/current-user-version", &out_appid, &out_url);
	ASSERT_STREQ("com.test.good_app1_1.2.3", out_appid);
	EXPECT_EQ(nullptr, out_url);

	dispatcher_send_to_app(out_appid, out_url);
	EXPECT_STREQ("com.test.good_app1_1.2.3", ubuntu_app_launch_mock_get_last_app_id());

	ubuntu_app_launch_mock_clear_last_app_id();
	g_clear_pointer(&out_appid, g_free);
	out_url = nullptr;

	/* Last app */
	dispatcher_url_to_appid("appid://com.test.good/last-listed-app/current-user-version", &out_appid, &out_url);
	ASSERT_STREQ("com.test.good_app1_1.2.3", out_appid);
	EXPECT_EQ(nullptr, out_url);

	dispatcher_send_to_app(out_appid, out_url);
	EXPECT_STREQ("com.test.good_app1_1.2.3", ubuntu_app_launch_mock_get_last_app_id());

	ubuntu_app_launch_mock_clear_last_app_id();
	g_clear_pointer(&out_appid, g_free);
	out_url = nullptr;

	/* Only app */
	dispatcher_url_to_appid("appid://com.test.good/only-listed-app/current-user-version", &out_appid, &out_url);
	ASSERT_STREQ("com.test.good_app1_1.2.3", out_appid);
	EXPECT_EQ(nullptr, out_url);

	dispatcher_send_to_app(out_appid, out_url);
	EXPECT_STREQ("com.test.good_app1_1.2.3", ubuntu_app_launch_mock_get_last_app_id());

	ubuntu_app_launch_mock_clear_last_app_id();
	g_clear_pointer(&out_appid, g_free);
	out_url = nullptr;

	/* Wild app fixed version */
	dispatcher_url_to_appid("appid://com.test.good/only-listed-app/1.2.3", &out_appid, &out_url);
	ASSERT_STREQ("com.test.good_app1_1.2.3", out_appid);
	EXPECT_EQ(nullptr, out_url);

	dispatcher_send_to_app(out_appid, out_url);
	EXPECT_STREQ("com.test.good_app1_1.2.3", ubuntu_app_launch_mock_get_last_app_id());
	ubuntu_app_launch_mock_clear_last_app_id();
	g_clear_pointer(&out_appid, g_free);
	out_url = nullptr;

	return;
}

TEST_F(AppIdTest, OrderingUrl)
{
	gchar * out_appid = nullptr;
	const gchar * out_url = nullptr;

	dispatcher_url_to_appid("appid://com.test.multiple/first-listed-app/current-user-version", &out_appid, &out_url);
	ASSERT_STREQ("com.test.multiple_app-first_1.2.3", out_appid);
	EXPECT_EQ(nullptr, out_url);

	dispatcher_send_to_app(out_appid, out_url);
	EXPECT_STREQ("com.test.multiple_app-first_1.2.3", ubuntu_app_launch_mock_get_last_app_id());
	ubuntu_app_launch_mock_clear_last_app_id();

	g_clear_pointer(&out_appid, g_free);
	out_url = nullptr;

	dispatcher_url_to_appid("appid://com.test.multiple/last-listed-app/current-user-version", &out_appid, &out_url);

	ASSERT_STREQ("com.test.multiple_app-last_1.2.3", out_appid);
	EXPECT_EQ(nullptr, out_url);

	dispatcher_send_to_app(out_appid, out_url);
	EXPECT_STREQ("com.test.multiple_app-last_1.2.3", ubuntu_app_launch_mock_get_last_app_id());

	ubuntu_app_launch_mock_clear_last_app_id();
	g_clear_pointer(&out_appid, g_free);
	out_url = nullptr;

	dispatcher_url_to_appid("appid://com.test.multiple/only-listed-app/current-user-version", &out_appid, &out_url);

	EXPECT_EQ(nullptr, out_appid);
	EXPECT_EQ(nullptr, out_url);

	return;
}

TEST_F(AppIdTest, BadDirectory)
{
	gchar * out_appid = nullptr;
	const gchar * out_url = nullptr;

	g_setenv("TEST_CLICK_DB", "not-click-db", TRUE);

	dispatcher_url_to_appid("appid://com.test.good/app1/current-user-version", &out_appid, &out_url);

	EXPECT_EQ(nullptr, out_appid);
	EXPECT_EQ(nullptr, out_url);

	return;
}

TEST_F(AppIdTest, BadUser)
{
	gchar * out_appid = nullptr;
	const gchar * out_url = nullptr;

	g_setenv("TEST_CLICK_USER", "not-test-user", TRUE);

	dispatcher_url_to_appid("appid://com.test.good/app1/current-user-version", &out_appid, &out_url);

	EXPECT_EQ(nullptr, out_appid);
	EXPECT_EQ(nullptr, out_url);
}
