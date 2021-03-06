/*
 * returnToBasic:
 *	A new implementation of BASIC
 *	Copyright (c) 2012 Gordon Henderson
 *********************************************************************************
 * piFns.h:
 *	Functions to handle the Raspberry Pi's GPIO pins
 *********************************************************************************
 * This file is part of RTB:
 *	Return To Basic
 *	http://projects.drogon.net/return-to-basic
 *
 *    RTB is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    RTB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with RTB.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************
 */

extern int doDigitalRead  (void *ptr) ;
extern int doDigitalWrite (void *ptr) ;
extern int doPinMode      (void *ptr) ;
extern int doPullUpDn     (void *ptr) ;
extern int doPwmWrite     (void *ptr) ;
