/* Fixed course names, par, tee/hole positions and ramp settings. Maintain alongside the Tiled layouts. */
#include "config.h"
#include "course.h"

const course_info_t course_infos[GAME_COURSE_COUNT] = {
    { 2, 21, 37, 2, 4, "FIRST BEND" }, /* course 0: ball=(2,21), hole=(37,2), par=4 */
    { 2, 22, 35, 21, 7, "POND SLALOM" }, /* course 1: ball=(2,22), hole=(35,21), par=7 */
    { 5, 18, 36, 5, 7, "RAMP CHANNEL" }, /* course 2: ball=(5,18), hole=(36,5), par=7 */
    { 34, 17, 3, 6, 7, "DRY SWITCHBACK" }, /* course 3: ball=(34,17), hole=(3,6), par=7 */
    { 9, 19, 9, 4, 4, "CRESCENT COVE" }, /* course 4: ball=(9,19), hole=(9,4), par=4 */
    { 5, 19, 34, 4, 4, "PINWHEEL COURT" }, /* course 5: ball=(5,19), hole=(34,4), par=4 */
    { 5, 19, 34, 4, 4, "RIBBON RIVER" }, /* course 6: ball=(5,19), hole=(34,4), par=4 */
    { 19, 20, 32, 10, 4, "LOTUS LAGOON" }, /* course 7: ball=(19,20), hole=(32,10), par=4 */
    { 5, 18, 34, 5, 3, "MIRROR GALLERY" }, /* course 8: ball=(5,18), hole=(34,5), par=3 */
    { 19, 21, 19, 4, 3, "KEYHOLE KEEP" }, /* course 9: ball=(19,21), hole=(19,4), par=3 */
    { 5, 18, 34, 5, 4, "TWIN COMETS" }, /* course 10: ball=(5,18), hole=(34,5), par=4 */
    { 5, 19, 34, 5, 5, "ARCHIPELAGO" }, /* course 11: ball=(5,19), hole=(34,5), par=5 */
    { 8, 20, 31, 3, 3, "HOURGLASS GARDEN" }, /* course 12: ball=(8,20), hole=(31,3), par=3 */
    { 4, 20, 14, 9, 8, "NAUTILUS" }, /* course 13: ball=(4,20), hole=(14,9), par=8 */
    { 5, 20, 34, 4, 5, "AURORA CROWN" }, /* course 14: ball=(5,20), hole=(34,4), par=5 */
};
