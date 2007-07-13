// makeswf -v 5 -r 1 -o date-5.swf date.as
// makeswf -v 6 -r 1 -o date-6.swf date.as
// makeswf -v 7 -r 1 -o date-7.swf date.as

// TODO: set*

#include "values.as"

function traced (d)
{
  trace (d.getFullYear () + "-" + d.getMonth () + "-" + d.getDate () + " " + d.getHours () + ":" + d.getMinutes () + ":" + d.getSeconds () + "." + d.getMilliseconds ());
}

function traced_utc (d)
{
  trace (d.getTime () + " " + d.valueOf ());
  trace (d.getUTCFullYear () + "-" + d.getUTCMonth () + "-" + d.getUTCDate () + " " + d.getUTCHours () + ":" + d.getUTCMinutes () + ":" + d.getUTCSeconds () + "." + d.getUTCMilliseconds ());
}

function p (val)
{
  return val + " (" + typeof (val) + ")";
}

extraValues = [-0.6, -0.4, 1.4, 1.6];

yearValues = [1983, 83, 0, 100, 99, 101, -100, -500, -271822, -271821, 275760, 275759, 100000000000000, -100000000000000];
yearValues = yearValues.concat (values).concat (extraValues);
monthValues = [5, 0, 11, 12, -1, 20, -20];
monthValues = monthValues.concat (values).concat (extraValues);
dateValues = [25, 1, 10, 29, 30, 31, 32, 0, -1, 200, -20];
dateValues = dateValues.concat (values).concat (extraValues);
hourValues = [13, 0, 5, 19, 23, -1, 24, 30, -20];
hourValues = hourValues.concat (values).concat (extraValues);
minuteValues = [50, 0, 10, 48, 59, -1, 60, -20, 200];
secondValues = minuteValues = minuteValues.concat (values).concat (extraValues);
millisecondValues = [300, 0, 12, 123, 580, 900, 934, 999, -1, 1000, 3123, -213];
millisecondValues = millisecondValues.concat (values).concat (extraValues);

// Date.UTC

for (i = 0; i < yearValues.length; i++) {
  trace ("Date.UTC (" + p (yearValues[i]) +")");
  val = Date.UTC (yearValues[i]);
  trace (val);
  if (val != undefined)
    traced_utc (new Date (val));
  trace ("Date.UTC (" + p (yearValues[i]) + ", " + monthValues[0] + ", " + dateValues[0] + ", " +
      hourValues[0] + ", " + minuteValues[0] + ", " + secondValues[0] + ", " +
      millisecondValues[0] + ")");
  val = Date.UTC (yearValues[i], monthValues[0], dateValues[0], hourValues[0], minuteValues[0],
    secondValues[0], millisecondValues[0]);
  trace (val);
  if (val != undefined)
    traced_utc (new Date (val));
}

for (i = 0; i < monthValues.length; i++) {
  trace ("Date.UTC (" + yearValues[0] + ", " + p (monthValues[i]) + ")");
  trace (Date.UTC (yearValues[0], monthValues[i]));
  trace ("Date.UTC (" + yearValues[0] + ", " + p (monthValues[i]) + ", " + dateValues[0] + ", " +
      hourValues[0] + ", " + minuteValues[0] + ", " + secondValues[0] + ", " +
      millisecondValues[0] + ")");
  trace (Date.UTC (yearValues[0], monthValues[i], dateValues[0], hourValues[0], minuteValues[0],
    secondValues[0], millisecondValues[0]));
}

for (i = 0; i < dateValues.length; i++) {
  trace ("Date.UTC (" + yearValues[0] + ", " + monthValues[0] + ", " + p (dateValues[i]) + ")");
  trace (Date.UTC (yearValues[0], monthValues[0], dateValues[i]));
  trace ("Date.UTC (" + yearValues[0] + ", " + monthValues[0] + ", " + p (dateValues[i]) + ", " +
      hourValues[0] + ", " + minuteValues[0] + ", " + secondValues[0] + ", " +
      millisecondValues[0] + ")");
  trace (Date.UTC (yearValues[0], monthValues[0], dateValues[i], hourValues[0], minuteValues[0],
    secondValues[0], millisecondValues[0]));
}

for (i = 0; i < hourValues.length; i++) {
  trace ("Date.UTC (" + yearValues[0] + ", " + monthValues[0] + ", " + dateValues[0] +
      ", " + p (hourValues[i]) + ")");
  trace (Date.UTC (yearValues[0], monthValues[0], dateValues[0], hourValues[i]));
  trace ("Date.UTC (" + yearValues[0] + ", " + monthValues[0] + ", " + dateValues[0] + ", " +
      p (hourValues[i]) + ", " + minuteValues[0] + ", " + secondValues[0] + ", " +
      millisecondValues[0] + ")");
  trace (Date.UTC (yearValues[0], monthValues[0], dateValues[0], hourValues[i], minuteValues[0],
    secondValues[0], millisecondValues[0]));
}

for (i = 0; i < minuteValues.length; i++) {
  trace ("Date.UTC (" + yearValues[0] + ", " + monthValues[0] + ", " + dateValues[0] + ", " +
      hourValues[0] + ", " + p (minuteValues[i]) + ")");
  trace (Date.UTC (yearValues[0], monthValues[0], dateValues[0], hourValues[0], minuteValues[i]));
  trace ("Date.UTC (" + yearValues[0] + ", " + monthValues[0] + ", " + dateValues[0] + ", " +
      hourValues[0] + ", " + p (minuteValues[i]) + ", " + secondValues[0] + ", " +
      millisecondValues[0] + ")");
  trace (Date.UTC (yearValues[0], monthValues[0], dateValues[0], hourValues[0], minuteValues[i],
    secondValues[0], millisecondValues[0]));
}

for (i = 0; i < secondValues.length; i++) {
  trace ("Date.UTC (" + yearValues[0] + ", " + monthValues[0] + ", " + dateValues[0] + ", " +
      hourValues[0] + ", " + minuteValues[0] + ", " + p (secondValues[i]) + ")");
  trace (Date.UTC (yearValues[0], monthValues[0], dateValues[0], hourValues[0], minuteValues[0],
      secondValues[i]));
  trace ("Date.UTC (" + yearValues[0] + ", " + monthValues[0] + ", " + dateValues[0] + ", " +
      hourValues[0] + ", " + minuteValues[0] + ", " + p (secondValues[i]) + ", " +
      millisecondValues[0] + ")");
  trace (Date.UTC (yearValues[0], monthValues[0], dateValues[0], hourValues[0], minuteValues[0],
    secondValues[i], millisecondValues[0]));
}

for (i = 0; i < millisecondValues.length; i++) {
  trace ("Date.UTC (" + yearValues[0] + ", " + monthValues[0] + ", " + dateValues[0] + ", " +
      hourValues[0] + ", " + minuteValues[0] + ", " + secondValues[0] + ", " +
      p (millisecondValues[i]) + ")");
  trace (Date.UTC (yearValues[0], monthValues[0], dateValues[0], hourValues[0], minuteValues[0],
    secondValues[0], millisecondValues[i]));
}

// new Date ()

for (i = 0; i < yearValues.length; i++) {
  if (yearValues[i] == undefined) // we can't test undefined first parameter, returns current time
    continue;
  trace ("new Date (" + p (yearValues[i]) + ")");
  traced (new Date (yearValues[i]));
  trace ("new Date (" + p (yearValues[i]) + ", " + monthValues[0] + ", " + dateValues[0] + ", " +
      hourValues[0] + ", " + minuteValues[0] + ", " + secondValues[0] + ", " +
      millisecondValues[0] + ")");
  traced (new Date (yearValues[i], monthValues[0], dateValues[0], hourValues[0], minuteValues[0],
    secondValues[0], millisecondValues[0]));
}

for (i = 0; i < monthValues.length; i++) {
  trace ("new Date (" + yearValues[0] + ", " + p (monthValues[i]) + ")");
  traced (new Date (yearValues[0], monthValues[i]));
  trace ("new Date (" + yearValues[0] + ", " + p (monthValues[i]) + ", " + dateValues[0] + ", " +
      hourValues[0] + ", " + minuteValues[0] + ", " + secondValues[0] + ", " +
      millisecondValues[0] + ")");
  traced (new Date (yearValues[0], monthValues[i], dateValues[0], hourValues[0], minuteValues[0],
    secondValues[0], millisecondValues[0]));
}

for (i = 0; i < dateValues.length; i++) {
  trace ("new Date (" + yearValues[0] + ", " + monthValues[0] + ", " + p (dateValues[i]) + ")");
  traced (new Date (yearValues[0], monthValues[0], dateValues[i]));
  trace ("new Date (" + yearValues[0] + ", " + monthValues[0] + ", " + p (dateValues[i]) + ", " +
      hourValues[0] + ", " + minuteValues[0] + ", " + secondValues[0] + ", " +
      millisecondValues[0] + ")");
  traced (new Date (yearValues[0], monthValues[0], dateValues[i], hourValues[0], minuteValues[0],
    secondValues[0], millisecondValues[0]));
}

for (i = 0; i < hourValues.length; i++) {
  trace ("new Date (" + yearValues[0] + ", " + monthValues[0] + ", " + dateValues[i] + ", " +
      p (hourValues[i]) + ")");
  traced (new Date (yearValues[0], monthValues[0], dateValues[0], hourValues[i]));
  trace ("new Date (" + yearValues[0] + ", " + monthValues[0] + ", " + dateValues[0] + ", " +
      p (hourValues[i]) + ", " + minuteValues[0] + ", " + secondValues[0] + ", " +
      millisecondValues[0] + ")");
  traced (new Date (yearValues[0], monthValues[0], dateValues[0], hourValues[i], minuteValues[0],
    secondValues[0], millisecondValues[0]));
}

for (i = 0; i < minuteValues.length; i++) {
  trace ("new Date (" + yearValues[0] + ", " + monthValues[0] + ", " + dateValues[i] + ", " +
      hourValues[0] + ", " + p (minuteValues[i]) + ")");
  traced (new Date (yearValues[0], monthValues[0], dateValues[0], hourValues[0], minuteValues[i]));
  trace ("new Date (" + yearValues[0] + ", " + monthValues[0] + ", " + dateValues[0] + ", " +
      hourValues[0] + ", " + p (minuteValues[i]) + ", " + secondValues[0] + ", " +
      millisecondValues[0] + ")");
  traced (new Date (yearValues[0], monthValues[0], dateValues[0], hourValues[0], minuteValues[i],
    secondValues[0], millisecondValues[0]));
}

for (i = 0; i < secondValues.length; i++) {
  trace ("new Date (" + yearValues[0] + ", " + monthValues[0] + ", " + dateValues[i] + ", " +
      hourValues[0] + ", " + minuteValues[0] + ", " + p (secondValues[i]) + ")");
  traced (new Date (yearValues[0], monthValues[0], dateValues[0], hourValues[0], minuteValues[0],
      secondValues[i]));
  trace ("new Date (" + yearValues[0] + ", " + monthValues[0] + ", " + dateValues[0] + ", " +
      hourValues[0] + ", " + minuteValues[0] + ", " + p (secondValues[i]) + ", " +
      millisecondValues[0] + ")");
  traced (new Date (yearValues[0], monthValues[0], dateValues[0], hourValues[0], minuteValues[0],
    secondValues[i], millisecondValues[0]));
}

for (i = 0; i < millisecondValues.length; i++) {
  trace ("new Date (" + yearValues[0] + ", " + monthValues[0] + ", " + dateValues[0] + ", " +
      hourValues[0] + ", " + minuteValues[0] + ", " + secondValues[0] + ", " +
      p (millisecondValues[0]) + ")");
  traced (new Date (yearValues[0], monthValues[0], dateValues[0], hourValues[0], minuteValues[0],
    secondValues[0], millisecondValues[i]));
}

// new Date ()

num_test_dates = 2;
function test_date (num)
{
  // FIXME: we don't check < 1970 year values, because there is weird flashplayer
  // bug that is not copied by swfdec (yet)
  if (num == 0) {
    return new Date (yearValues[0], monthValues[0], dateValues[0], hourValues[0], minuteValues[0],
	secondValues[0], millisecondValues[0]);
  } else {
    return new Date (2005, 8, 20, NaN);
  }
}

for (var num = 0; num < num_test_dates; num++) {
  for (var i = 0; i < yearValues.length; i++) {
    var d = test_date (num);
    trace ("test_date (" + num + ").setYear (" + p (yearValues[i]) + ")");
    traced (d);
    trace (d.setYear (yearValues[i]));
    traced (d);
  }
}

for (var num = 0; num < num_test_dates; num++) {
  for (var i = 0; i < monthValues.length; i++) {
    var d = test_date (num);
    trace ("test_date (" + num + ").setMonth (" + p (monthValues[i]) + ")");
    traced (d);
    trace (d.setMonth (monthValues[i]));
    traced (d);
  }
}

loadMovie ("FSCommand:quit", "");
