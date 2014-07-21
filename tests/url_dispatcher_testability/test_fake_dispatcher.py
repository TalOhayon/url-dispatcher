# -*- Mode: Python; coding: utf-8; indent-tabs-mode: nil; tab-width: 4 -*-
#
# Copyright (C) 2014 Canonical Ltd.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; version 3.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

import testtools
from url_dispatcher_testability import fixture_setup, fake_dispatcher

from subprocess import call, CalledProcessError


class FakeDispatcherTestCase(testtools.TestCase):

    def setUp(self):
        super(FakeDispatcherTestCase, self).setUp()
        self.dispatcher = fixture_setup.FakeURLDispatcher()
        self.useFixture(self.dispatcher)

    def test_url_dispatcher(self):
        call(['../../tools/url-dispatcher', 'test://testurl'])
        try:
            last_url = self.dispatcher.get_last_dispatch_url_call_parameter()
        except fake_dispatcher.FakeDispatcherException:
            last_url = None
        self.assertEqual(last_url, 'test://testurl')
