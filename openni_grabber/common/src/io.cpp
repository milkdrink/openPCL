/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2010-2011, Willow Garage, Inc.
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: io.cpp 4681 2012-02-22 23:48:44Z mdixon $
 *
 */
#include "stdafx.h"
#include "pcl/point_types.h"
#include "pcl/common/io.h"

//////////////////////////////////////////////////////////////////////////
void
getFieldsSizes (const std::vector<sensor_msgs::PointField> &fields,
                std::vector<int> &fields_sizes)
{
  int valid = 0;
  fields_sizes.resize (fields.size ());
  for (size_t i = 0; i < fields.size (); ++i)
  {
    if (fields[i].name == "_")
      continue;

    int fs = fields[i].count * pcl::getFieldSize (fields[i].datatype);
    fields_sizes[i] = fs;
    valid++;
  }
  fields_sizes.resize (valid);
}

bool fieldComp (const sensor_msgs::PointField* i, const sensor_msgs::PointField* j)
{
  return i->offset < j->offset;
}

//////////////////////////////////////////////////////////////////////////
bool
pcl::concatenateFields (const sensor_msgs::PointCloud2 &cloud1, 
                        const sensor_msgs::PointCloud2 &cloud2, 
                        sensor_msgs::PointCloud2 &cloud_out)
{
  // If the cloud's sizes differ (points wise), then exit with error
  if (cloud1.width != cloud2.width || cloud1.height != cloud2.height)
    return (false);

  if (cloud1.is_bigendian != cloud2.is_bigendian)
    return (false);

  // Else, copy the second cloud (width, height, header stay the same)
  // we do this since fields from the second cloud are supposed to overwrite
  // those of the first
  cloud_out.header = cloud2.header;
  cloud_out.fields = cloud2.fields;
  cloud_out.width = cloud2.width;
  cloud_out.height = cloud2.height;
  cloud_out.is_bigendian = cloud2.is_bigendian;

  //We need to find how many fields overlap between the two clouds
  size_t total_fields = cloud2.fields.size ();

  //for the non-matching fields in cloud1, we need to store the offset
  //from the beginning of the point
  std::vector<const sensor_msgs::PointField*> cloud1_unique_fields;
  std::vector<int> field_sizes;

  //We need to make sure that the fields for cloud 1 are sorted
  //by offset so that we can compute sizes correctly. There is no
  //guarantee that the fields are in the correct order when they come in
  std::vector<const sensor_msgs::PointField*> cloud1_fields_sorted;
  for (size_t i = 0; i < cloud1.fields.size (); ++i)
    cloud1_fields_sorted.push_back (&(cloud1.fields[i]));

  std::sort (cloud1_fields_sorted.begin (), cloud1_fields_sorted.end (), fieldComp);

  for (size_t i = 0; i < cloud1_fields_sorted.size (); ++i)
  {
    bool match = false;
    for (size_t j = 0; j < cloud2.fields.size (); ++j)
    {
      if (cloud1_fields_sorted[i]->name == cloud2.fields[j].name)
        match = true;
    }

    //if the field is new, we'll increment out total fields
    if (!match && cloud1_fields_sorted[i]->name != "_")
    {
      cloud1_unique_fields.push_back (cloud1_fields_sorted[i]);

      int size = 0;
      size_t next_valid_field = i + 1;

      while (next_valid_field < cloud1_fields_sorted.size())
      {
        if (cloud1_fields_sorted[next_valid_field]->name != "_")
          break;
        next_valid_field++;
      }

      if (next_valid_field < cloud1_fields_sorted.size ())
        //compute the true size of the field, including padding
        size = cloud1_fields_sorted[next_valid_field]->offset - cloud1_fields_sorted[i]->offset;
      else
        //for the last point, we'll just use the point step to compute the size
        size = cloud1.point_step - cloud1_fields_sorted[i]->offset;

      field_sizes.push_back (size);

      total_fields++;
    }
  }

  //we need to compute the size of the additional data added from cloud 1
  uint32_t cloud1_unique_point_step = 0;
  for (size_t i = 0; i < cloud1_unique_fields.size (); ++i)
    cloud1_unique_point_step += field_sizes[i];

  //the total size of extra data should be the size of data per point
  //multiplied by the total number of points in the cloud
  uint32_t cloud1_unique_data_size = cloud1_unique_point_step * cloud1.width * cloud1.height; 

  // Point step must increase with the length of each matching field
  cloud_out.point_step = cloud2.point_step + cloud1_unique_point_step;
  // Recalculate row_step
  cloud_out.row_step = cloud_out.point_step * cloud_out.width;

  // Resize data to hold all clouds
  cloud_out.data.resize (cloud2.data.size () + cloud1_unique_data_size);

  // Concatenate fields
  cloud_out.fields.resize (cloud2.fields.size () + cloud1_unique_fields.size ());
  int offset = cloud2.point_step;

  for (size_t d = 0; d < cloud1_unique_fields.size (); ++d)
  {
    const sensor_msgs::PointField& f = *cloud1_unique_fields[d];
    cloud_out.fields[cloud2.fields.size () + d].name = f.name;
    cloud_out.fields[cloud2.fields.size () + d].datatype = f.datatype;
    cloud_out.fields[cloud2.fields.size () + d].count = f.count;
    // Adjust the offset
    cloud_out.fields[cloud2.fields.size () + d].offset = offset;
    offset += field_sizes[d];
  }
 
  // Iterate over each point and perform the appropriate memcpys
  int point_offset = 0;
  for (size_t cp = 0; cp < cloud_out.width * cloud_out.height; ++cp)
  {
    memcpy (&cloud_out.data[point_offset], &cloud2.data[cp * cloud2.point_step], cloud2.point_step);
    int field_offset = cloud2.point_step;

    // Copy each individual point, we have to do this on a per-field basis
    // since some fields are not unique
    for (size_t i = 0; i < cloud1_unique_fields.size (); ++i)
    {
      const sensor_msgs::PointField& f = *cloud1_unique_fields[i];
      int local_data_size = f.count * pcl::getFieldSize (f.datatype);
      int padding_size = field_sizes[i] - local_data_size;
      
      memcpy (&cloud_out.data[point_offset + field_offset], &cloud1.data[cp * cloud1.point_step + f.offset], local_data_size);
      field_offset +=  local_data_size;

      //make sure that we add padding when its needed
      if (padding_size > 0)
        memset (&cloud_out.data[point_offset + field_offset], 0, padding_size);
      field_offset += padding_size;
    }
    point_offset += field_offset;
  }

  if (!cloud1.is_dense || !cloud2.is_dense)
    cloud_out.is_dense = false;
  else
    cloud_out.is_dense = true;

  return (true);
}

//////////////////////////////////////////////////////////////////////////
bool
pcl::concatenatePointCloud (const sensor_msgs::PointCloud2 &cloud1, 
                            const sensor_msgs::PointCloud2 &cloud2, 
                            sensor_msgs::PointCloud2 &cloud_out)
{
  if (cloud1.fields.size () != cloud2.fields.size ())
    return (false);

  for (size_t i = 0; i < cloud1.fields.size (); ++i)
    if (cloud1.fields[i].name != cloud2.fields[i].name)
      return (false);

  // Copy cloud1 into cloud_out
  cloud_out = cloud1;
  size_t nrpts = cloud_out.data.size ();
  cloud_out.data.resize (nrpts + cloud2.data.size ());
  memcpy (&cloud_out.data[nrpts], &cloud2.data[0], cloud2.data.size ());

  // Height = 1 => no more organized
  cloud_out.width    = cloud1.width * cloud1.height + cloud2.width * cloud2.height;
  cloud_out.height   = 1;
  if (!cloud1.is_dense || !cloud2.is_dense)
    cloud_out.is_dense = false;
  else
    cloud_out.is_dense = true;

  return (true);
}

//////////////////////////////////////////////////////////////////////////
bool
pcl::getPointCloudAsEigen (const sensor_msgs::PointCloud2 &in, Eigen::MatrixXf &out)
{
  // Get X-Y-Z indices
  int x_idx = getFieldIndex (in, "x");
  int y_idx = getFieldIndex (in, "y");
  int z_idx = getFieldIndex (in, "z");

  if (x_idx == -1 || y_idx == -1 || z_idx == -1)
  {
    PCL_ERROR ("Input dataset has no X-Y-Z coordinates! Cannot convert to Eigen format.\n");
    return (false);
  }

  if (in.fields[x_idx].datatype != sensor_msgs::PointField::FLOAT32 || 
      in.fields[y_idx].datatype != sensor_msgs::PointField::FLOAT32 || 
      in.fields[z_idx].datatype != sensor_msgs::PointField::FLOAT32)
  {
    PCL_ERROR ("X-Y-Z coordinates not floats. Currently only floats are supported.\n");
    return (false);
  }

  size_t npts = in.width * in.height;
  out = Eigen::MatrixXf::Ones (4, npts);

  Eigen::Array4i xyz_offset (in.fields[x_idx].offset, in.fields[y_idx].offset, in.fields[z_idx].offset, 0);

  // Copy the input dataset into Eigen format
  for (size_t i = 0; i < npts; ++i)
  {
     // Unoptimized memcpys: assume fields x, y, z are in random order
     memcpy (&out (0, i), &in.data[xyz_offset[0]], sizeof (float));
     memcpy (&out (1, i), &in.data[xyz_offset[1]], sizeof (float));
     memcpy (&out (2, i), &in.data[xyz_offset[2]], sizeof (float));

     xyz_offset += in.point_step;
  }

  return (true);
}

//////////////////////////////////////////////////////////////////////////
bool 
pcl::getEigenAsPointCloud (Eigen::MatrixXf &in, sensor_msgs::PointCloud2 &out)
{
  // Get X-Y-Z indices
  int x_idx = getFieldIndex (out, "x");
  int y_idx = getFieldIndex (out, "y");
  int z_idx = getFieldIndex (out, "z");

  if (x_idx == -1 || y_idx == -1 || z_idx == -1)
  {
    PCL_ERROR ("Output dataset has no X-Y-Z coordinates set up as fields! Cannot convert from Eigen format.\n");
    return (false);
  }

  if (out.fields[x_idx].datatype != sensor_msgs::PointField::FLOAT32 || 
      out.fields[y_idx].datatype != sensor_msgs::PointField::FLOAT32 || 
      out.fields[z_idx].datatype != sensor_msgs::PointField::FLOAT32)
  {
    PCL_ERROR ("X-Y-Z coordinates not floats. Currently only floats are supported.\n");
    return (false);
  }

  if (in.cols () != (int)(out.width * out.height))
  {
    PCL_ERROR ("Number of points in the point cloud differs from the Eigen matrix. Cannot continue.\n");
    return (false);
  }

  size_t npts = in.cols ();

  Eigen::Array4i xyz_offset (out.fields[x_idx].offset, out.fields[y_idx].offset, out.fields[z_idx].offset, 0);

  // Copy the input dataset into Eigen format
  for (size_t i = 0; i < npts; ++i)
  {
     // Unoptimized memcpys: assume fields x, y, z are in random order
     memcpy (&out.data[xyz_offset[0]], &in (0, i), sizeof (float));
     memcpy (&out.data[xyz_offset[1]], &in (1, i), sizeof (float));
     memcpy (&out.data[xyz_offset[2]], &in (2, i), sizeof (float));

     xyz_offset += out.point_step;
  }

  return (true);
}

//////////////////////////////////////////////////////////////////////////
void 
pcl::copyPointCloud (
    const sensor_msgs::PointCloud2 &cloud_in, const std::vector<int> &indices, 
    sensor_msgs::PointCloud2 &cloud_out)
{
  cloud_out.header       = cloud_in.header;
  cloud_out.height       = 1;
  cloud_out.width        = (uint32_t) indices.size (); 
  cloud_out.fields       = cloud_in.fields;
  cloud_out.is_bigendian = cloud_in.is_bigendian;
  cloud_out.point_step   = cloud_in.point_step;
  cloud_out.row_step     = cloud_in.row_step;
  if (cloud_in.is_dense)
    cloud_out.is_dense = true;
  else
    // It's not necessarily true that is_dense is false if cloud_in.is_dense is false
    // To verify this, we would need to iterate over all points and check for NaNs
    cloud_out.is_dense = false;

  cloud_out.data.resize (cloud_out.width * cloud_out.height * cloud_out.point_step);

  // Iterate over each point
  for (size_t i = 0; i < indices.size (); ++i)
  {
    memcpy (&cloud_out.data[i * cloud_out.point_step], &cloud_in.data[indices[i] * cloud_in.point_step], cloud_in.point_step);
    // Check for NaNs, set is_dense to true/false based on this
    // ...
  }
}
