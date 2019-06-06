/*
 *  parameter.h
 *
 *  This file is part of NEST.
 *
 *  Copyright (C) 2004 The NEST Initiative
 *
 *  NEST is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  NEST is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with NEST.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef PARAMETER_H_
#define PARAMETER_H_

// C++ includes:
#include <limits>
#include <math.h>

// Includes from librandom:
#include "normal_randomdev.h"
#include "randomgen.h"

// Includes from nestkernel:
#include "nest_names.h"
#include "nest_types.h"
#include "nestmodule.h"

// Includes from sli:
#include "dictutils.h"

namespace nest
{

/**
 * Abstract base class for parameters
 */
class Parameter
{
public:
  /**
   * Default constructor
   */
  Parameter()
  {
  }

  /**
   * Constructor
   * Parameter that can be set in the Dictionary:
   * @param d dictionary with parameter values
   */
  Parameter( const DictionaryDatum& d )
  {
  }

  /**
   * Virtual destructor
   */
  virtual ~Parameter()
  {
  }

  /**
   * @returns the value of the parameter.
   */
  virtual double value( librandom::RngPtr& rng, Node* node ) const = 0;
  virtual double value( librandom::RngPtr& rng,
    index sgid,
    Node* target,
    thread target_thread ) const
    {
      return value( rng, nullptr );
    }
  
  virtual double value( librandom::RngPtr& rng,  const std::vector< double >& source_pos, 
    const std::vector< double >& target_pos  ) const
  {
    return value( rng, nullptr);
  }

  /**
   * Clone method.
   * @returns dynamically allocated copy of parameter object
   */
  virtual Parameter* clone() const = 0;

  /**
   * Create the product of this parameter with another.
   * @returns a new dynamically allocated parameter.
   */
  virtual Parameter* multiply_parameter( const Parameter& other ) const;
  /**
   * Create the quotient of this parameter with another.
   * @returns a new dynamically allocated parameter.
   */
  virtual Parameter* divide_parameter( const Parameter& other ) const;
  /**
   * Create the sum of this parameter with another.
   * @returns a new dynamically allocated parameter.
   */
  virtual Parameter* add_parameter( const Parameter& other ) const;
  /**
   * Create the difference of this parameter with another.
   * @returns a new dynamically allocated parameter.
   */
  virtual Parameter* subtract_parameter( const Parameter& other ) const;
  /**
   * Create comparison of this parameter with another.
   * @returns a new dynamically allocated parameter.
   */
  virtual Parameter* compare_parameter( const Parameter& other,
    const DictionaryDatum& d ) const;
  /**
   * Create parameter choosing between two other parameters,
   * based on this parameter.
   * @returns a new dynamically allocated parameter.
   */
  virtual Parameter* conditional_parameter( const Parameter& if_true,
    const Parameter& if_false ) const;
  /**
   * Create the exponential of this parameter.
   * @returns a new dynamically allocated parameter.
   */
  virtual Parameter* exp() const;
  /**
   * Create the sine of this parameter.
   * @returns a new dynamically allocated parameter.
   */
  virtual Parameter* sin() const;
  /**
   * Create the cosine of this parameter.
   * @returns a new dynamically allocated parameter.
   */
  virtual Parameter* cos() const;

  /**
   * Create TODO
   * @returns a new dynamically allocated parameter.
   */
  virtual Parameter* dimension_parameter( const Parameter& y_parameter ) const;
  virtual Parameter* dimension_parameter( const Parameter& y_parameter,
    const Parameter& z_parameter ) const;

protected:
  Node* gid_to_node_ptr_( const index, const thread ) const;
};

/**
 * Parameter with constant value.
 */
class ConstantParameter : public Parameter
{
public:
  ConstantParameter( double value )
    : Parameter()
    , value_( value )
  {
  }

  /**
   * Parameters:
   * value - constant value of this parameter
   */
  ConstantParameter( const DictionaryDatum& d )
    : Parameter( d )
  {
    value_ = getValue< double >( d, "value" );
  }

  ~ConstantParameter()
  {
  }

  /**
   * @returns the constant value of this parameter.
   */
  double
  value( librandom::RngPtr&, Node* ) const
  {
    return value_;
  }

  Parameter*
  clone() const
  {
    return new ConstantParameter( value_ );
  }

private:
  double value_;
};


/**
 * Random parameter with uniform distribution in [min,max)
 */
class UniformParameter : public Parameter
{
public:
  /**
   * Parameters:
   * min - minimum value
   * max - maximum value
   */
  UniformParameter( const DictionaryDatum& d )
    : Parameter( d )
    , lower_( 0.0 )
    , range_( 1.0 )
  {
    updateValue< double >( d, names::min, lower_ );
    updateValue< double >( d, names::max, range_ );
    if ( lower_ >= range_ )
    {
      throw BadProperty(
        "nest::UniformParameter: "
        "min < max required." );
    }

    range_ -= lower_;
  }

  double
  value( librandom::RngPtr& rng, Node* ) const
  {
    return lower_ + rng->drand() * range_;
  }

  Parameter*
  clone() const
  {
    return new UniformParameter( *this );
  }

private:
  double lower_, range_;
};


/**
 * Random parameter with normal distribution, optionally truncated to [min,max).
 * Truncation is implemented by rejection.
 */
class NormalParameter : public Parameter
{
public:
  /**
   * Parameters:
   * mean  - mean value
   * sigma - standard distribution
   * min   - minimum value
   * max   - maximum value
   */
  NormalParameter( const DictionaryDatum& d )
    : Parameter( d )
    , mean_( 0.0 )
    , sigma_( 1.0 )
    , min_( -std::numeric_limits< double >::infinity() )
    , max_( std::numeric_limits< double >::infinity() )
    , rdev()
  {
    updateValue< double >( d, names::mean, mean_ );
    updateValue< double >( d, names::sigma, sigma_ );
    updateValue< double >( d, names::min, min_ );
    updateValue< double >( d, names::max, max_ );
    if ( sigma_ <= 0 )
    {
      throw BadProperty(
        "nest::NormalParameter: "
        "sigma > 0 required." );
    }
    if ( min_ >= max_ )
    {
      throw BadProperty(
        "nest::NormalParameter: "
        "min < max required." );
    }
  }

  double
  value( librandom::RngPtr& rng, Node* ) const
  {
    double val;
    do
    {
      val = mean_ + rdev( rng ) * sigma_;
    } while ( ( val < min_ ) or ( val >= max_ ) );
    return val;
  }

  Parameter*
  clone() const
  {
    return new NormalParameter( *this );
  }

private:
  double mean_, sigma_, min_, max_;
  librandom::NormalRandomDev rdev;
};


/**
 * Random parameter with lognormal distribution, optionally truncated to
 * [min,max). Truncation is implemented by rejection.
 */
class LognormalParameter : public Parameter
{
public:
  /**
   * Parameters:
   * mu    - mean value of logarithm
   * sigma - standard distribution of logarithm
   * min   - minimum value
   * max   - maximum value
   */
  LognormalParameter( const DictionaryDatum& d )
    : Parameter( d )
    , mu_( 0.0 )
    , sigma_( 1.0 )
    , min_( -std::numeric_limits< double >::infinity() )
    , max_( std::numeric_limits< double >::infinity() )
    , rdev()
  {
    updateValue< double >( d, names::mu, mu_ );
    updateValue< double >( d, names::sigma, sigma_ );
    updateValue< double >( d, names::min, min_ );
    updateValue< double >( d, names::max, max_ );
    if ( sigma_ <= 0 )
    {
      throw BadProperty(
        "nest::LognormalParameter: "
        "sigma > 0 required." );
    }
    if ( min_ >= max_ )
    {
      throw BadProperty(
        "nest::LognormalParameter: "
        "min < max required." );
    }
  }

  double
  value( librandom::RngPtr& rng, Node* ) const
  {
    double val;
    do
    {
      val = std::exp( mu_ + rdev( rng ) * sigma_ );
    } while ( ( val < min_ ) or ( val >= max_ ) );
    return val;
  }

  Parameter*
  clone() const
  {
    return new LognormalParameter( *this );
  }

private:
  double mu_, sigma_, min_, max_;
  librandom::NormalRandomDev rdev;
};


/**
 * Exponential parameter.
 */
class ExponentialParameter : public Parameter
{
public:
  /**
   * Parameters:
   * scale - the scale parameter
   */
  ExponentialParameter( const DictionaryDatum& d )
    : Parameter( d )
    , scale_( 1.0 )
  {
    updateValue< double >( d, names::scale, scale_ );
  }

  double
  value( librandom::RngPtr& rng, Node* ) const
  {
    return scale_ * ( -std::log( 1 - rng->drand() ) );
  }

  Parameter*
  clone() const
  {
    return new ExponentialParameter( *this );
  }

private:
  double scale_;
};


/**
 * Node position parameter.
 */
class NodePosParameter : public Parameter
{
public:
  /**
   * Parameters:
   * dimension - Dimension from which to get the position value of the node.
   * type_id - If specified, specifies if the position should be taken from the
   *           presynaptic or postsynaptic node in a connection.
   *           0: unspecified, 1: presynaptic, 2: postsynaptic.
   */
  NodePosParameter( const DictionaryDatum& d )
    : Parameter( d )
    , dimension_( 0 )
    , node_location_( 0 )
  {
    updateValue< long >( d, names::dimension, dimension_ );
    updateValue< long >(
      d, names::type_id, node_location_ ); // TODO: Better name than "type_id"?
    if ( node_location_ < 0 or 2 < node_location_ )
    {
      throw BadParameterValue(
        "Node location must either be unspecified (0), source (1) or target "
        "(2)" );
    }
  }

  double
  value( librandom::RngPtr& rng, Node* node ) const
  {
    if ( node_location_ != 0 )
    {
      throw BadParameterValue(
        "Source or target position parameter can only be used when "
        "connecting." );
    }
    return get_node_pos_( rng, node );
  }

  double
  value( librandom::RngPtr& rng,
    index sgid,
    Node* target,
    thread target_thread ) const
  {
    switch ( node_location_ )
    {
    case 0:
      throw BadParameterValue(
        "Node position parameter cannot be used when connecting." );
    case 1:
    {
      Node* source = gid_to_node_ptr_( sgid, target_thread );
      return get_node_pos_( rng, source );
    }
    case 2:
      return get_node_pos_( rng, target );
    }
    // TODO: assert that we don't get here
    throw KernelException( "Wrong node_location_." );
  }

  double value( librandom::RngPtr& rng, const std::vector< double >& source_pos, 
    const std::vector< double >& target_pos ) const
  {
    switch ( node_location_ )
    {
    case 0:
      throw BadParameterValue(
        "Node position parameter cannot be used when connecting." );
    case 1:
    {
      return source_pos[dimension_];
    }
    case 2:
      return target_pos[dimension_];
    }
    // TODO: assert that we don't get here
    throw KernelException( "Wrong node_location_." );
  }

  Parameter*
  clone() const
  {
    return new NodePosParameter( *this );
  }

private:
  int dimension_;
  int node_location_;

  double get_node_pos_( librandom::RngPtr& rng, Node* node ) const;
};


/**
 * Node distance parameter.
 */
class SpatialDistanceParameter : public Parameter
{
public:
  SpatialDistanceParameter( const DictionaryDatum& d )
    : Parameter( d )
  {
  }

  double
  value( librandom::RngPtr& rng, Node* ) const
  {
    throw BadParameterValue(
      "Spatial distance parameter can only be used when connecting." );
  }

  double value( librandom::RngPtr&, index, Node*, thread ) const;

  double value( librandom::RngPtr& rng, const std::vector< double >& source_pos, 
    const std::vector< double >& target_pos ) const ;

  Parameter*
  clone() const
  {
    return new SpatialDistanceParameter( *this );
  }
};


/**
 * Parameter class representing the product of two parameters
 */
class ProductParameter : public Parameter
{
public:
  /**
   * Construct the product of the two given parameters. Copies are made
   * of the supplied Parameter objects.
   */
  ProductParameter( const Parameter& m1, const Parameter& m2 )
    : Parameter()
    , parameter1_( m1.clone() )
    , parameter2_( m2.clone() )
  {
  }

  /**
   * Copy constructor.
   */
  ProductParameter( const ProductParameter& p )
    : Parameter( p )
    , parameter1_( p.parameter1_->clone() )
    , parameter2_( p.parameter2_->clone() )
  {
  }

  ~ProductParameter()
  {
    delete parameter1_;
    delete parameter2_;
  }

  /**
   * @returns the value of the product.
   */
  double
  value( librandom::RngPtr& rng, Node* node ) const
  {
    return parameter1_->value( rng, node ) * parameter2_->value( rng, node );
  }

  double
  value( librandom::RngPtr& rng,
    index sgid,
    Node* target,
    thread target_thread ) const
  {
    return parameter1_->value( rng, sgid, target, target_thread )
      * parameter2_->value( rng, sgid, target, target_thread );
  }

  Parameter*
  clone() const
  {
    return new ProductParameter( *this );
  }

protected:
  Parameter* parameter1_, *parameter2_;
};

/**
 * Parameter class representing the quotient of two parameters
 */
class QuotientParameter : public Parameter
{
public:
  /**
   * Construct the quotient of the two given parameters. Copies are made
   * of the supplied Parameter objects.
   */
  QuotientParameter( const Parameter& m1, const Parameter& m2 )
    : Parameter()
    , parameter1_( m1.clone() )
    , parameter2_( m2.clone() )
  {
  }

  /**
   * Copy constructor.
   */
  QuotientParameter( const QuotientParameter& p )
    : Parameter( p )
    , parameter1_( p.parameter1_->clone() )
    , parameter2_( p.parameter2_->clone() )
  {
  }

  ~QuotientParameter()
  {
    delete parameter1_;
    delete parameter2_;
  }

  /**
   * @returns the value of the product.
   */
  double
  value( librandom::RngPtr& rng, Node* node ) const
  {
    return parameter1_->value( rng, node ) / parameter2_->value( rng, node );
  }

  double
  value( librandom::RngPtr& rng,
    index sgid,
    Node* target,
    thread target_thread ) const
  {
    return parameter1_->value( rng, sgid, target, target_thread )
      / parameter2_->value( rng, sgid, target, target_thread );
  }

  Parameter*
  clone() const
  {
    return new QuotientParameter( *this );
  }

protected:
  Parameter* parameter1_, *parameter2_;
};

/**
 * Parameter class representing the sum of two parameters
 */
class SumParameter : public Parameter
{
public:
  /**
   * Construct the sum of the two given parameters. Copies are made
   * of the supplied Parameter objects.
   */
  SumParameter( const Parameter& m1, const Parameter& m2 )
    : Parameter()
    , parameter1_( m1.clone() )
    , parameter2_( m2.clone() )
  {
  }

  /**
   * Copy constructor.
   */
  SumParameter( const SumParameter& p )
    : Parameter( p )
    , parameter1_( p.parameter1_->clone() )
    , parameter2_( p.parameter2_->clone() )
  {
  }

  ~SumParameter()
  {
    delete parameter1_;
    delete parameter2_;
  }

  /**
   * @returns the value of the sum.
   */
  double
  value( librandom::RngPtr& rng, Node* node ) const
  {
    return parameter1_->value( rng, node ) + parameter2_->value( rng, node );
  }

  double
  value( librandom::RngPtr& rng,
    index sgid,
    Node* target,
    thread target_thread ) const
  {
    return parameter1_->value( rng, sgid, target, target_thread )
      + parameter2_->value( rng, sgid, target, target_thread );
  }

  Parameter*
  clone() const
  {
    return new SumParameter( *this );
  }

protected:
  Parameter* parameter1_, *parameter2_;
};

/**
 * Parameter class representing the difference of two parameters
 */
class DifferenceParameter : public Parameter
{
public:
  /**
   * Construct the difference of the two given parameters. Copies are made
   * of the supplied Parameter objects.
   */
  DifferenceParameter( const Parameter& m1, const Parameter& m2 )
    : Parameter()
    , parameter1_( m1.clone() )
    , parameter2_( m2.clone() )
  {
  }

  /**
   * Copy constructor.
   */
  DifferenceParameter( const DifferenceParameter& p )
    : Parameter( p )
    , parameter1_( p.parameter1_->clone() )
    , parameter2_( p.parameter2_->clone() )
  {
  }

  ~DifferenceParameter()
  {
    delete parameter1_;
    delete parameter2_;
  }

  /**
   * @returns the value of the difference.
   */
  double
  value( librandom::RngPtr& rng, Node* node ) const
  {
    return parameter1_->value( rng, node ) - parameter2_->value( rng, node );
  }

  double
  value( librandom::RngPtr& rng,
    index sgid,
    Node* target,
    thread target_thread ) const
  {
    return parameter1_->value( rng, sgid, target, target_thread )
      - parameter2_->value( rng, sgid, target, target_thread );
  }

  Parameter*
  clone() const
  {
    return new DifferenceParameter( *this );
  }

protected:
  Parameter* parameter1_, *parameter2_;
};

/**
 * Parameter class for a parameter oriented in the opposite direction.
 */
class ConverseParameter : public Parameter
{
public:
  /**
   * Construct the converse of the given parameter. A copy is made of the
   * supplied Parameter object.
   */
  ConverseParameter( const Parameter& p )
    : Parameter( p )
    , p_( p.clone() )
  {
  }

  /**
   * Copy constructor.
   */
  ConverseParameter( const ConverseParameter& p )
    : Parameter( p )
    , p_( p.p_->clone() )
  {
  }

  ~ConverseParameter()
  {
    delete p_;
  }

  /**
   * @returns the value of the parameter.
   */
  double
  value( librandom::RngPtr& rng, Node* node ) const
  {
    return p_->value( rng, node );
  }

  double
  value( librandom::RngPtr& rng,
    index sgid,
    Node* target,
    thread target_thread ) const
  {
    return p_->value( rng, sgid, target, target_thread );
  }

  Parameter*
  clone() const
  {
    return new ConverseParameter( *this );
  }

protected:
  Parameter* p_;
};


/**
 * Parameter class representing the comparison of two parameters.
 */
class ComparingParameter : public Parameter
{
public:
  /**
   * Construct the comparison of the two given parameters. Copies are made
   * of the supplied Parameter objects.
   *
   * comparator - Operator to use as a comparator.
   *              0: <
   *              2: <=
   *              4: ==
   *              5: !=
   *              3: >=
   *              1: >
   *
   */
  ComparingParameter( const Parameter& m1,
    const Parameter& m2,
    const DictionaryDatum& d )
    : Parameter()
    , parameter1_( m1.clone() )
    , parameter2_( m2.clone() )
    , comparator_( -1 )
  {
    if ( not updateValue< long >( d, names::comparator, comparator_ ) )
    {
      throw BadParameter( "A comparator has to be specified." );
    }
    if ( comparator_ < 0 or 5 < comparator_ )
    {
      throw BadParameter(
        "Comparator specification has to be in the range 0-5." );
    }
  }

  /**
   * Copy constructor.
   */
  ComparingParameter( const ComparingParameter& p )
    : Parameter( p )
    , parameter1_( p.parameter1_->clone() )
    , parameter2_( p.parameter2_->clone() )
    , comparator_( p.comparator_ )
  {
  }

  ~ComparingParameter()
  {
    delete parameter1_;
    delete parameter2_;
  }

  /**
   * @returns the result of the comparison, bool given as a double.
   */
  double
  value( librandom::RngPtr& rng, Node* node ) const
  {
    return compare_(
      parameter1_->value( rng, node ), parameter2_->value( rng, node ) );
  }

  double
  value( librandom::RngPtr& rng,
    index sgid,
    Node* target,
    thread target_thread ) const
  {
    return compare_( parameter1_->value( rng, sgid, target, target_thread ),
      parameter2_->value( rng, sgid, target, target_thread ) );
  }


  Parameter*
  clone() const
  {
    return new ComparingParameter( *this );
  }

protected:
  Parameter* parameter1_, *parameter2_;

private:
  bool
  compare_( double value_a, double value_b ) const
  {
    switch ( comparator_ )
    {
    case 0:
      return value_a < value_b;
    case 1:
      return value_a <= value_b;
    case 2:
      return value_a == value_b;
    case 3:
      return value_a != value_b;
    case 4:
      return value_a >= value_b;
    case 5:
      return value_a > value_b;
    }
    throw KernelException( "Wrong comparison operator." );
  }

  int comparator_;
};


/**
 * Parameter class choosing a value based on a comparing parameter.
 */
class ConditionalParameter : public Parameter
{
public:
  /**
   * Construct the choice of two given parameters, based on a third.
   * Copies are made of the supplied Parameter objects.
   */
  ConditionalParameter( const Parameter& condition,
    const Parameter& if_true,
    const Parameter& if_false )
    : Parameter()
    , condition_( condition.clone() )
    , if_true_( if_true.clone() )
    , if_false_( if_false.clone() )
  {
  }

  /**
   * Copy constructor.
   */
  ConditionalParameter( const ConditionalParameter& p )
    : Parameter( p )
    , condition_( p.condition_->clone() )
    , if_true_( p.if_true_->clone() )
    , if_false_( p.if_false_->clone() )
  {
  }

  ~ConditionalParameter()
  {
    delete condition_;
    delete if_true_;
    delete if_false_;
  }

  /**
   * @returns the value chosen by the comparison.
   */
  double
  value( librandom::RngPtr& rng, Node* node ) const
  {
    if ( condition_->value( rng, node ) )
    {
      return if_true_->value( rng, node );
    }
    else
    {
      return if_false_->value( rng, node );
    }
  }

  double
  value( librandom::RngPtr& rng,
    index sgid,
    Node* target,
    thread target_thread ) const
  {
    if ( condition_->value( rng, sgid, target, target_thread ) )
    {
      return if_true_->value( rng, sgid, target, target_thread );
    }
    else
    {
      return if_false_->value( rng, sgid, target, target_thread );
    }
  }


  Parameter*
  clone() const
  {
    return new ConditionalParameter( *this );
  }

protected:
  Parameter* condition_, *if_true_, *if_false_;
};


/**
 * Parameter class representing the exponential of a parameter.
 */
class ExpParameter : public Parameter
{
public:
  /**
   * Construct the exponential of the given parameter. A copy is made of the
   * supplied Parameter object.
   */
  ExpParameter( const Parameter& p )
    : Parameter( p )
    , p_( p.clone() )
  {
  }

  /**
   * Copy constructor.
   */
  ExpParameter( const ExpParameter& p )
    : Parameter( p )
    , p_( p.p_->clone() )
  {
  }

  ~ExpParameter()
  {
    delete p_;
  }

  /**
   * @returns the value of the parameter.
   */
  double
  value( librandom::RngPtr& rng, Node* node ) const
  {
    return std::exp( p_->value( rng, node ) );
  }

  double
  value( librandom::RngPtr& rng,
    index sgid,
    Node* target,
    thread target_thread ) const
  {
    return std::exp( p_->value( rng, sgid, target, target_thread ) );
  }

  Parameter*
  clone() const
  {
    return new ExpParameter( *this );
  }

protected:
  Parameter* p_;
};


/**
 * Parameter class representing the sine of a parameter.
 */
class SinParameter : public Parameter
{
public:
  /**
   * Construct the sine of the given parameter. A copy is made of the
   * supplied Parameter object.
   */
  SinParameter( const Parameter& p )
    : Parameter( p )
    , p_( p.clone() )
  {
  }

  /**
   * Copy constructor.
   */
  SinParameter( const SinParameter& p )
    : Parameter( p )
    , p_( p.p_->clone() )
  {
  }

  ~SinParameter()
  {
    delete p_;
  }

  /**
   * @returns the value of the parameter.
   */
  double
  value( librandom::RngPtr& rng, Node* node ) const
  {
    return std::sin( p_->value( rng, node ) );
  }

  double
  value( librandom::RngPtr& rng,
    index sgid,
    Node* target,
    thread target_thread ) const
  {
    return std::sin( p_->value( rng, sgid, target, target_thread ) );
  }

  Parameter*
  clone() const
  {
    return new SinParameter( *this );
  }

protected:
  Parameter* p_;
};

/**
 * Parameter class representing the cosine of a parameter.
 */
class CosParameter : public Parameter
{
public:
  /**
   * Construct the exponential of the given parameter. A copy is made of the
   * supplied Parameter object.
   */
  CosParameter( const Parameter& p )
    : Parameter( p )
    , p_( p.clone() )
  {
  }

  /**
   * Copy constructor.
   */
  CosParameter( const CosParameter& p )
    : Parameter( p )
    , p_( p.p_->clone() )
  {
  }

  ~CosParameter()
  {
    delete p_;
  }

  /**
   * @returns the value of the parameter.
   */
  double
  value( librandom::RngPtr& rng, Node* node ) const
  {
    return std::cos( p_->value( rng, node ) );
  }

  double
  value( librandom::RngPtr& rng,
    index sgid,
    Node* target,
    thread target_thread ) const
  {
    return std::cos( p_->value( rng, sgid, target, target_thread ) );
  }

  Parameter*
  clone() const
  {
    return new CosParameter( *this );
  }

protected:
  Parameter* p_;
};


/**
 * Parameter class representing .
 */
class DimensionParameter : public Parameter
{
public:
  /**
   * Construct the exponential of the given parameter. A copy is made of the
   * supplied Parameter object.
   */
  DimensionParameter( const Parameter& px, const Parameter& py )
    : num_dimensions_( 2 )
    , px_( px.clone() )
    , py_( py.clone() )
  {
  }

  DimensionParameter( const Parameter& px,
    const Parameter& py,
    const Parameter& pz )
    : num_dimensions_( 3 )
    , px_( px.clone() )
    , py_( py.clone() )
    , pz_( pz.clone() )
  {
  }

  /**
   * Copy constructor.
   */
  DimensionParameter( const DimensionParameter& p )
    : num_dimensions_( p.num_dimensions_ )
    , px_( p.px_->clone() )
    , py_( p.py_->clone() )
    , pz_( p.pz_->clone() )
  {
  }

  ~DimensionParameter()
  {
    delete px_;
    delete py_;
    if ( num_dimensions_ == 3 )
    {
      delete pz_;
    }
  }

  /**
   * @returns the value of the parameter.
   */
  double
  value( librandom::RngPtr& rng, Node* node ) const
  {
    throw KernelException( "Cannot get value of DimensionParameter." );
  }

  double
  value( librandom::RngPtr& rng,
    index sgid,
    Node* target,
    thread target_thread ) const
  {
    throw KernelException( "Cannot get value of DimensionParameter." );
  }

  std::vector< double >
  get_values( librandom::RngPtr& rng )
  {
    switch ( num_dimensions_ )
    {
    case 2:
      return { px_->value( rng, nullptr ), py_->value( rng, nullptr ) };
    case 3:
      return { px_->value( rng, nullptr ),
        py_->value( rng, nullptr ),
        pz_->value( rng, nullptr ) };
    }
    throw KernelException( "Wrong number of dimensions in get_values!" );
  }

  int
  get_num_dimensions() const
  {
    return num_dimensions_;
  }

  Parameter*
  clone() const
  {
    return new DimensionParameter( *this );
  }

protected:
  int num_dimensions_;
  Parameter* px_;
  Parameter* py_;
  Parameter* pz_;
};


inline Parameter*
Parameter::multiply_parameter( const Parameter& other ) const
{
  return new ProductParameter( *this, other );
}

inline Parameter*
Parameter::divide_parameter( const Parameter& other ) const
{
  return new QuotientParameter( *this, other );
}

inline Parameter*
Parameter::add_parameter( const Parameter& other ) const
{
  return new SumParameter( *this, other );
}

inline Parameter*
Parameter::subtract_parameter( const Parameter& other ) const
{
  return new DifferenceParameter( *this, other );
}

inline Parameter*
Parameter::compare_parameter( const Parameter& other,
  const DictionaryDatum& d ) const
{
  return new ComparingParameter( *this, other, d );
}

inline Parameter*
Parameter::conditional_parameter( const Parameter& if_true,
  const Parameter& if_false ) const
{
  return new ConditionalParameter( *this, if_true, if_false );
}

inline Parameter*
Parameter::exp() const
{
  return new ExpParameter( *this );
}

inline Parameter*
Parameter::sin() const
{
  return new SinParameter( *this );
}

inline Parameter*
Parameter::cos() const
{
  return new CosParameter( *this );
}


inline Parameter*
Parameter::dimension_parameter( const Parameter& y_parameter ) const
{
  return new DimensionParameter( *this, y_parameter );
}

inline Parameter*
Parameter::dimension_parameter( const Parameter& y_parameter,
  const Parameter& z_parameter ) const
{
  return new DimensionParameter( *this, y_parameter, z_parameter );
}

} // namespace nest

#endif
