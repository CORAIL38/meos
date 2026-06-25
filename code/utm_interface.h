#pragma once

namespace utm {
  // LatLonToUTMXY
  // Converts a latitude/longitude pair to x and y coordinates in the
  // Universal Transverse Mercator projection.
  //
  // Inputs:
  //   lat - Latitude of the point, in radians.
  //   lon - Longitude of the point, in radians.
  //   zone - UTM zone to be used for calculating values for x and y.
  //          If zone is less than 1 or greater than 60, the routine
  //          will determine the appropriate zone from the value of lon.
  //
  // Outputs:
  //   x - The x coordinate (easting) of the computed point. (in meters)
  //   y - The y coordinate (northing) of the computed point. (in meters)
  //
  // Returns:
  //   The UTM zone used for calculating the values of x and y.
  int LatLonToUTMXY(double lat, double lon, int zone, double& x, double& y);

  // UTMXYToLatLon
  //
  // Converts x and y coordinates in the Universal Transverse Mercator//   The UTM zone parameter should be in the range [1,60].

  // projection to a latitude/longitude pair.
  //
  // Inputs:
  // x - The easting of the point, in meters.
  // y - The northing of the point, in meters.
  // zone - The UTM zone in which the point lies.
  // southhemi - True if the point is in the southern hemisphere;
  //               false otherwise.
  //
  // Outputs:
  // lat - The latitude of the point, in radians.
  // lon - The longitude of the point, in radians.
  // 
  // Returns:
  // The function does not return a value.
  void UTMXYToLatLon(double x, double y, int zone, bool southhemi, double& lat, double& lon);
}
