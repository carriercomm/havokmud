/*
 *  This file is part of the havokmud package
 *  Copyright (C) 2013 Gavin Hurlbut
 *
 *  havokmud is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * @file
 * @brief HavokMud Thread base class
 */

#include "thread/HavokThread.hpp"
#include "thread/ThreadMap.hpp"
#include "thread/ThreadColors.hpp"
#include "thread/LoggingThread.hpp"

namespace {
    static const int DEFAULT_STACK_SIZE = 1 * 1024 * 1024;  // 1MB
}

namespace havokmud {
    namespace thread {

        HavokThread::HavokThread(std::string name) : m_name(name)
        {
            m_attrs.set_stack_size(DEFAULT_STACK_SIZE);
            m_thread = boost::thread(m_attrs,
                                     boost::bind(&HavokThread::prv_start,
                                                 this));
            //m_joiner = boost::thread_joiner(m_thread);
            m_id = boost::this_thread::get_id();
            
            m_color = ThreadColors();
            m_index = g_threadMap.addThread(this);
        }

        HavokThread::~HavokThread()
        {
            g_threadMap.removeThread(this);
        }
    }
}

