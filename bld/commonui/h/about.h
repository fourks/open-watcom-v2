/****************************************************************************
*
*                            Open Watcom Project
*
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  About dialog resource identifiers.
*
****************************************************************************/


#ifndef _ABOUT_H_INCLUDED
#define _ABOUT_H_INCLUDED

#define ABOUT_INFO1                 103
#define ABOUT_INFO2                 104
#define ABOUT_INFO3                 105
#define ABOUT_VERSION               106
#define ABOUT_NAME                  107
#define ABOUT_ICON                  108
#define ABOUT_COPYRIGHT             109
#define APPICON                     101

WINEXPORT INT_PTR CALLBACK AboutProc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam );

#endif /* _ABOUT_H_INCLUDED */
