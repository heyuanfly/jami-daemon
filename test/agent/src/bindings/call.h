/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
 *
 *  Author: Olivier Dion <olivier.dion@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#pragma once

/* Jami */
#include "jami/callmanager_interface.h"
#include "jami/configurationmanager_interface.h"

/* Agent */
#include "utils.h"

static SCM
place_call_binding(SCM accountID_str, SCM contact_str, SCM call_details_alist_optional)
{
    if (SCM_UNBNDP(call_details_alist_optional)) {
        return to_guile(DRing::placeCall(from_guile(accountID_str),
                                         from_guile(contact_str)));
    }

    return to_guile(DRing::placeCall(from_guile(accountID_str),
                                     from_guile(contact_str),
                                     static_cast<std::map<std::string, std::string>>(from_guile(call_details_alist_optional))));
}

static SCM
place_call_with_media_binding(SCM accountID_str, SCM contact_str, SCM call_media_vector_alist_optional)
{
    if (SCM_UNBNDP(call_media_vector_alist_optional)) {
        call_media_vector_alist_optional = scm_c_make_vector(0, SCM_UNDEFINED);
    }

    return to_guile(DRing::placeCallWithMedia(from_guile(accountID_str),
                                              from_guile(contact_str),
                                              from_guile(call_media_vector_alist_optional)));
}

static SCM
hang_up_binding(SCM callID_str)
{
    return to_guile(DRing::hangUp(from_guile(callID_str)));
}

static SCM
accept_binding(SCM callID_str, SCM call_media_vector_alist_optional)
{
    if (SCM_UNBNDP(call_media_vector_alist_optional)) {
        return to_guile(DRing::accept(from_guile(callID_str)));
    }

    return to_guile(DRing::acceptWithMedia(from_guile(callID_str),
                                           from_guile(call_media_vector_alist_optional)));
}

static SCM
refuse_binding(SCM callID_str)
{
    return to_guile(DRing::refuse(from_guile(callID_str)));
}

static SCM
hold_binding(SCM callID_str)
{
    return to_guile(DRing::hold(from_guile(callID_str)));
}

static SCM
unhold_binding(SCM callID_str)
{
    return to_guile(DRing::unhold(from_guile(callID_str)));
}

static void
install_call_primitives(void *)
{
    define_primitive("place-call", 2, 1, 0, (void*) place_call_binding);
    define_primitive("place-call/media", 2, 1, 0, (void*) place_call_with_media_binding);
    define_primitive("hang-up", 1, 0, 0, (void*) hang_up_binding);
    define_primitive("accept", 1, 1, 0, (void*) accept_binding);
    define_primitive("refuse", 1, 0, 0, (void*) refuse_binding);
    define_primitive("hold", 1, 0, 0, (void*) hold_binding);
    define_primitive("unhold", 1, 0, 0, (void*) unhold_binding);
}