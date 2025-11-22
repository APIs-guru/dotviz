#include "geom.h"
#include "geomprocs.h"
#include "types.h"
#include "utils.h"
#include <stdbool.h>

#define HW 2.0 /* maximum distance away from line, in points */

/* check_control_points function checks the size of quadrilateral
 * formed by four control points
 * returns true if four points are in line (or close to line)
 * else return false
 */
static bool check_control_points(pointf *cp) {
  double dis1 = ptToLine2(cp[0], cp[3], cp[1]);
  double dis2 = ptToLine2(cp[0], cp[3], cp[2]);
  return dis1 < HW * HW && dis2 < HW * HW;
}

/* update bounding box to contain a bezier segment */
void update_bb_bz(boxf *bb, pointf *cp) {

  /* if any control point of the segment is outside the bounding box */
  if (cp[0].x > bb->UR.x || cp[0].x < bb->LL.x || cp[0].y > bb->UR.y ||
      cp[0].y < bb->LL.y || cp[1].x > bb->UR.x || cp[1].x < bb->LL.x ||
      cp[1].y > bb->UR.y || cp[1].y < bb->LL.y || cp[2].x > bb->UR.x ||
      cp[2].x < bb->LL.x || cp[2].y > bb->UR.y || cp[2].y < bb->LL.y ||
      cp[3].x > bb->UR.x || cp[3].x < bb->LL.x || cp[3].y > bb->UR.y ||
      cp[3].y < bb->LL.y) {

    /* if the segment is sufficiently refined */
    if (check_control_points(cp)) {
      int i;
      /* expand the bounding box */
      for (i = 0; i < 4; i++) {
        if (cp[i].x > bb->UR.x)
          bb->UR.x = cp[i].x;
        else if (cp[i].x < bb->LL.x)
          bb->LL.x = cp[i].x;
        if (cp[i].y > bb->UR.y)
          bb->UR.y = cp[i].y;
        else if (cp[i].y < bb->LL.y)
          bb->LL.y = cp[i].y;
      }
    } else { /* else refine the segment */
      pointf left[4], right[4];
      Bezier(cp, 0.5, left, right);
      update_bb_bz(bb, left);
      update_bb_bz(bb, right);
    }
  }
}
