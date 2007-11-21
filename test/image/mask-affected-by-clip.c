/* gcc `pkg-config --libs --cflags libming` mask-affected-by-clip.c -o mask-affected-by-clip && ./mask-affected-by-clip
 */

#include <ming.h>

static SWFBlock
get_rectangle (int r, int g, int b)
{
  SWFMovieClip clip;
  SWFShape shape;
  SWFFillStyle fill;

  clip = newSWFMovieClip ();
  shape = newSWFShape ();
  fill = SWFShape_addSolidFillStyle (shape, r, g, b, 255);
  SWFShape_setRightFillStyle (shape, fill);
  SWFShape_drawLineTo (shape, 100, 0);
  SWFShape_drawLineTo (shape, 100, 100);
  SWFShape_drawLineTo (shape, 0, 100);
  SWFShape_drawLineTo (shape, 0, 0);

  SWFMovieClip_add (clip, (SWFBlock) shape);
  SWFMovieClip_nextFrame (clip);
  return (SWFBlock) clip;
}

static void
do_movie (int version)
{
  char name[100];
  SWFMovie movie;
  SWFDisplayItem item;

  movie = newSWFMovieWithVersion (version);
  SWFMovie_setRate (movie, 1);
  SWFMovie_setDimension (movie, 200, 150);

  item = SWFMovie_add (movie, get_rectangle (255, 0, 0));
  SWFDisplayItem_setDepth (item, 0);
  SWFDisplayItem_setMaskLevel (item, 2);

  item = SWFMovie_add (movie, get_rectangle (0, 255, 0));
  SWFDisplayItem_moveTo (item, 50, 25);
  SWFDisplayItem_setDepth (item, 1);
  SWFDisplayItem_setName (item, "b");

  item = SWFMovie_add (movie, get_rectangle (0, 0, 255));
  SWFDisplayItem_moveTo (item, 25, 50);
  SWFDisplayItem_setDepth (item, 3);
  SWFDisplayItem_setName (item, "c");

  SWFMovie_add (movie, (SWFBlock) newSWFAction (
	"c.setMask (b);"
	));
  SWFMovie_nextFrame (movie);

  sprintf (name, "mask-affected-by-clip-%d.swf", version);
  SWFMovie_save (movie, name);
}

int
main (int argc, char **argv)
{
  int i;

  if (Ming_init ())
    return 1;

  for (i = 8; i >= 5; i--) {
    do_movie (i);
  }

  return 0;
}
