/*
 * Copyright 2013  Giulio Camuffo <giuliocamuffo@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>

#include "settings.h"

Option::BindingValue::BindingValue(Binding::Type t, uint32_t f, uint32_t s)
                    : type(t)
                    , first(f)
                    , second(s)
{
}

Option::BindingValue Option::BindingValue::key(uint32_t key, weston_keyboard_modifier modifier)
{
    return BindingValue(Binding::Type::Key, key, modifier);
}

Option::BindingValue Option::BindingValue::axis(uint32_t axis, weston_keyboard_modifier modifier)
{
    return BindingValue(Binding::Type::Axis, axis, modifier);
}

Option::BindingValue Option::BindingValue::hotSpot(Binding::HotSpot hs)
{
    return BindingValue(Binding::Type::HotSpot, (uint32_t)hs, 0);
}

void Option::BindingValue::bind(Binding *b) const
{
    switch (type) {
        case Binding::Type::Key:
            b->bindKey(first, (weston_keyboard_modifier)second);
            break;
        case Binding::Type::Axis:
            b->bindAxis(first, (weston_keyboard_modifier)second);
            break;
        case Binding::Type::HotSpot:
            b->bindHotSpot((Binding::HotSpot)first);
            break;
    }
}

Option::Option(const char *n, const char *v)
      : m_name(n)
      , m_type(Type::String)
{
    m_defaultValue.string = v;
}

Option::Option(const char *n, int v)
      : m_name(n)
      , m_type(Type::Int)
{
    m_defaultValue.integer = v;
}

Option::Option(const char *n, Binding::Type a, const BindingValue &v)
      : m_name(n)
      , m_type(Type::Binding)
      , m_allowableBinding(a)
{
    m_defaultValue.binding = v;
}

Option::Option(const Option &o)
{
    *this = o;
}

Option &Option::operator=(const Option &o)
{
    m_name = o.m_name;
    m_type = o.m_type;
    m_allowableBinding = o.m_allowableBinding;
    switch (m_type) {
        case Type::String: m_defaultValue.string = o.m_defaultValue.string; break;
        case Type::Int: m_defaultValue.integer = o.m_defaultValue.integer; break;
        case Type::Binding: m_defaultValue.binding = o.m_defaultValue.binding; break;
    }

    return *this;
}

std::string Option::valueAsString() const
{
    assert(m_type == Type::String);
    return m_value.string;
}

int Option::valueAsInt() const
{
    assert(m_type == Type::Int);
    return m_value.integer;
}

const Option::BindingValue &Option::valueAsBinding() const
{
    assert(m_type == Type::Binding);
    return m_value.binding;
}




Settings::Settings(const char *group)
        : m_group(group)
{
}

const Option *Settings::option(const std::string &name) const
{
    auto it = m_options.find(name);
    if (it == m_options.end()) {
        return nullptr;
    }

    return &it->second;
}



std::unordered_map<std::string, Settings *> SettingsManager::s_settings;

bool SettingsManager::addSettings(Settings *s)
{
    auto opts = s->options();
    for (const Option &o: opts) {
        s->m_options.insert(std::pair<std::string, Option>(o.m_name, o));
    }

    s_settings[s->m_group + "/" + s->m_name] = s;
    return true;
}

bool SettingsManager::set(const char *path, const char *option, const std::string &v)
{
    Settings *s = s_settings[path];
    if (s) {
        auto it = s->m_options.find(option);
        if (it != s->m_options.end() && it->second.m_type == Option::Type::String) {
            it->second.m_value.string = v;
            s->set(option, v);
            return true;
        }
    }
    return false;
}

bool SettingsManager::set(const char *path, const char *option, int v)
{
    Settings *s = s_settings[path];
    if (s) {
        auto it = s->m_options.find(option);
        if (it != s->m_options.end() && it->second.m_type == Option::Type::Int) {
            it->second.m_value.integer = v;
            s->set(option, v);
            return true;
        }
    }
    return false;
}

bool SettingsManager::set(const char *path, const char *option, const Option::BindingValue &v)
{
    Settings *s = s_settings[path];
    if (s) {
        auto it = s->m_options.find(option);
        if (it != s->m_options.end() && it->second.m_type == Option::Type::Binding && it->second.m_allowableBinding & v.type) {
            it->second.m_value.binding = v;
            s->set(option, v);
            return true;
        }
    }
    return false;
}

void SettingsManager::init()
{
    for (auto &i: s_settings) {
        Settings *s = i.second;
        for (auto &o: s->m_options) {
            switch (o.second.m_type) {
                case Option::Type::String:
                    o.second.m_value.string = o.second.m_defaultValue.string;
                    s->set(o.first, o.second.m_value.string);
                    break;
                case Option::Type::Int:
                    o.second.m_value.integer = o.second.m_defaultValue.integer;
                    s->set(o.first, o.second.m_value.integer);
                    break;
                case Option::Type::Binding:
                    o.second.m_value.binding = o.second.m_defaultValue.binding;
                    s->set(o.first, o.second.m_value.binding);
                    break;
            }
        }
    }
}

void SettingsManager::cleanup()
{
    for (auto &s: s_settings) {
        delete s.second;
    }
}
