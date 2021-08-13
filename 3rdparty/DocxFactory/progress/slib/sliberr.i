
/**
 * sliberr.i -
 *
 * (c) Copyright ABC Alon Blich Consulting Tech, Ltd.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 *  Contact information
 *  Email: alonblich@gmail.com
 *  Phone: +972-54-218-8086
 */



&if defined( xSLibErr ) = 0 &then

    {slib/start-slib.i "'slib/sliberr.p'"}

    {slib/sliberrfrwd.i "in super"}



    &if "{1}" <> "" &then 

        run err_loadErrorMsgFile( {1} ).

        &global err_xErrorMsgFile {1}

    &endif

    &global xSLibErr defined

&endif /* defined = 0 */