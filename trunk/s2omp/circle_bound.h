/*
 * circle_bound.h
 *
 *  Created on: Jul 5, 2012
 *      Author: scranton
 */

#ifndef CIRCLE_BOUND_H_
#define CIRCLE_BOUND_H_

#include <stdint.h>
#include <math.h>
#include <vector>
#include <algorithm>
#include "core.h"
#include "point.h"
#include "bound_interface.h"
#include "MersenneTwister.h"

namespace s2omp {

class circle_bound : public bound_interface {
public:
  circle_bound();
  circle_bound(const point& axis, double height);
  virtual ~circle_bound();
  static double get_height_for_angle(double theta_radians);

  static circle_bound* from_angular_bin(const point& axis,
                                        const angular_bin& bin);
  static circle_bound* from_radius(const point& axis, double radius_degrees);
  static circle_bound* from_height(const point& axis, double height);

  inline point axis() const {
    return axis_;
  }
  // inline void set_axis(const point& p);
  inline double radius() const {
    return 2.0 * asin(sqrt(0.5 * height_)) * RAD_TO_DEG;
  }
  inline double height() const {
    return height_;
  }

  // circle_bound is the default object for describing the rough bound
  // for any bound_interface-derived object.  In order to return a circle_bound,
  // many objects will start with some center point at then expand the circle
  // to enclose other points.  Calling this method will increase the circle
  // height to enclose the input point.
  void add_point(const point& p);

  // Likewise, it can be useful to increase the bound to enclose other
  // circle_bounds
  void add_circle_bound(const circle_bound& bound);

  // Generate a random point in the circle_bound.  This method will be the
  // basis for the generic methods to generate random points for all
  // bound_interface objects.
  point create_random_point();

  void get_weighted_random_points(
      long n_points, point_vector* points, const point_vector& input_points);

  // API from bound_interface
  virtual bool is_empty() const;
  virtual long size() const;
  virtual void clear();
  virtual double area() const;

  virtual bool contains(const point& p) const;
  virtual bool contains(const pixel& pix) const;
  virtual double contained_area(const pixel& pix) const;

  virtual bool may_intersect(const pixel& pix) const;

  virtual point get_center() const {
    return axis_;
  }
  virtual circle_bound get_bound() const {
    return circle_bound(axis_, height_);
  }

  virtual point get_random_point() {
    return create_random_point();
  }

private:
  bool intersects(const pixel& pix, const point_vector& vertices) const;
  circle_bound* get_complement() const;
  void initialize_random();

  point axis_;
  double height_;

  // The variables below are for generating random points and will not be
  // initialized until initialize_random() is called.
  point great_circle_norm_;
  double rotate_;
  bool initialized_random_;
  MTRand mtrand_;
};

} // end namespace s2omp


#endif /* CIRCLE_BOUND_H_ */