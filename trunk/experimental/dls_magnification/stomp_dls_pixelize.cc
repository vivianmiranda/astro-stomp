#include <cstdio>
#include <cstdlib>
#include <string>
#include <iostream>
#include <sstream>
#include <stomp.h>
#include <gflags/gflags.h>

//Modified from Ryan Scranton's stomp_pixelize_circles.cc
//takes a file that defines circles and rectangles and generates a Stomp::Map file

//input regions file will contain 11 columns as follows
//index: index of region
//type:circle or square 
//radius/area: radius of cirlce or area of rectangle
//weight: pixel weight
//ra1 dec1: position cirlce center or first vertex of rectangle
//2-4: remaining verticies of rectangle, -99 if cirlce

// Define our command-line flags.
DEFINE_string(output_file, "",
              "Name of the ASCII file to store combined StompMap.");
DEFINE_string(input_file, "",
              "CSV list of ASCII file containing input regions");
DEFINE_string(stomp_file, "",
              "Name of the ASCII file maps should add/ingest to if at all");
DEFINE_int32(max_resolution, 2048,
	     "Maximum resolution to use when pixelizing regions.");
DEFINE_int32(start_index, 0,
	     "Starting circle index.");
DEFINE_int32(finish_index, -1,
	     "Last circle index (default to all)");
DEFINE_double(weight, 1.0,
	      "Default weight for the maps.");
DEFINE_bool(verbose, false,
	    "Print pixelization information for each circle individually.");
DEFINE_bool(add_maps, false,
	    "Add Weights of Maps");

int main(int argc, char **argv)
{
  std::string usage = "Usage: ";
  usage += argv[0];
  usage += " --output_file=<StompMap ASCII file>";
  usage += " --input_file=<ASCII circle parameter file>";
  google::SetUsageMessage(usage);
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_output_file.empty()) {
    std::cout << usage << "\n";
    std::cout << "Type '" << argv[0] << " --help' for a list of options.\n";
    exit(1);
  }

  if (FLAGS_input_file.empty()) {
    std::cout << usage << "\n";
    std::cout << "Type '" << argv[0] << " --help' for a list of options.\n";
    exit(1);
  }

  // First, we set up an empty Map to hold the pixelized Maps.
  Stomp::Map* exp_map;
  if (FLAGS_stomp_file.empty()) {
    exp_map = new Stomp::Map();
  }
  else {
    exp_map = new Stomp::Map(FLAGS_stomp_file);
    if (FLAGS_verbose) {
      std::cout<<"Input Stomp Map: "<<FLAGS_stomp_file<<", Area: "<<exp_map->Area()<<'\n';
    }
  }

  std::cout << "Parsing " << FLAGS_input_file << "...\n";
  std::ifstream region_file(FLAGS_input_file.c_str());
  int type;
  double radius;
  double ra[4],dec[4];
  unsigned long idx;
  double weight;

  if (!region_file.is_open()) {
    std::cout << FLAGS_input_file << " does not exist!  Exiting.\n";
    exit(1);
  }

  unsigned long n_regions = 0;
  unsigned long n_kept = 0;
  double raw_area = 0.0;
  double pixelized_raw_area = 0.0;
  unsigned long check = 1000;

  while (!region_file.eof()) {
    region_file >> idx >> type >> radius >> weight >> ra[0] >> dec[0] >> ra[1] >> dec[1] >> ra[2] >> dec[2] >> ra[3] >> dec[3];
    
    if (!region_file.eof()) {
      if (FLAGS_start_index <= idx &&
	  (FLAGS_finish_index == -1 || FLAGS_finish_index > idx)) {
	if (type==0) {
	  Stomp::AngularCoordinate ang(ra[0], dec[0],
				       Stomp::AngularCoordinate::Equatorial);
	  Stomp::CircleBound* circle_bound =
	    new Stomp::CircleBound(ang, radius);
	  raw_area += circle_bound->Area();
	  if (FLAGS_verbose) {
	    std::cout << idx << ", (" << ang.Lambda() << "," << ang.Eta() <<"), " << 
	      radius << ", " << circle_bound->Area() << ", " <<
	      circle_bound->LambdaMin() << " - " << circle_bound->LambdaMax() << ", " <<
	      circle_bound->EtaMin() << " - " << circle_bound->EtaMax() << "\n";
	  }
	  Stomp::Map* circle_map = 
	    new Stomp::Map(*circle_bound,weight,FLAGS_max_resolution,FLAGS_verbose);
	  pixelized_raw_area += circle_map->Area();
	  n_kept++;
	  if (FLAGS_add_maps)
	    exp_map->AddMap(*circle_map,false);
	  else
	    exp_map->IngestMap(*circle_map);
	  delete circle_bound;
	  delete circle_map;
	  n_regions++;
	}
	else if (type==3) {
	  Stomp::LatLonBound* latlon_bound = 
	    new Stomp::LatLonBound(dec[0], dec[1], ra[0], ra[1],
				   Stomp::AngularCoordinate::Equatorial);
	  raw_area += latlon_bound->Area();
	  if (FLAGS_verbose) {
	    std::cout << idx << " " << radius << ", " 
		      << latlon_bound->Area() << ", " 
		      << latlon_bound->LambdaMin() << " - " 
		      << latlon_bound->LambdaMax() << ", " 
		      << latlon_bound->EtaMin() << " - " 
		      << latlon_bound->EtaMax() << "\n";
	  }
	  Stomp::Map* latlon_map =
	    new Stomp::Map(*latlon_bound, weight, FLAGS_max_resolution,
			   FLAGS_verbose);
	  pixelized_raw_area += latlon_map->Area();
	  n_kept++;
	  if (FLAGS_add_maps)
	    exp_map->AddMap(*latlon_map, false);
	  else
	    exp_map->IngestMap(*latlon_map);
	  delete latlon_bound;
	  delete latlon_map;
	  n_regions++;
	}
	else if (type==4) {
	  Stomp::AngularVector ang;
	  for (int i=0;i<4;i++)
	    ang.push_back(Stomp::AngularCoordinate(ra[i],dec[i],Stomp::AngularCoordinate::Equatorial));
	  Stomp::PolygonBound* poly_bound = new Stomp::PolygonBound(ang);
	  if (FLAGS_verbose) {
	    std::cout << idx << ", [";
	    for (Stomp::AngularIterator iter=ang.begin();iter!=ang.end();iter++)
	      std::cout << "(" << (*iter).Lambda() << "," << (*iter).Eta() <<"), ";
	    std::cout << radius << "]: " << poly_bound->Area() << ", " <<
	      poly_bound->LambdaMin() << " - " << poly_bound->LambdaMax() << ", " <<
	      poly_bound->EtaMin() << " - " << poly_bound->EtaMax() << "\n";
	  }
	  Stomp::Map* poly_map = 
	    new Stomp::Map(*poly_bound,weight,FLAGS_max_resolution,FLAGS_verbose);
	  n_kept++;
	  
	  pixelized_raw_area += poly_map->Area();
	  raw_area += poly_bound->Area();
	  if (FLAGS_add_maps) 
	    exp_map->AddMap(*poly_map,false);
	  else
	    exp_map->IngestMap(*poly_map);

	  delete poly_map;
	  delete poly_bound;
	}
      }
      if (n_regions > 0 &&
	  (idx < FLAGS_finish_index || FLAGS_finish_index == -1) &&
	  n_regions % check == 0 && !FLAGS_verbose) {
	std::string status_file_name = FLAGS_output_file + "_status";
	std::ofstream status_file(status_file_name.c_str());
	status_file << idx << ": " << n_kept << "/" << n_regions << "/" <<
	  FLAGS_finish_index - FLAGS_start_index <<
	  " regions pixelized; " << pixelized_raw_area <<
	  " sq. degrees (" << raw_area << ")\n";
	status_file.close();
      }
    }//end region file check
  }//end of file reading
  region_file.close();
  if (exp_map->Size() > 0) {
    std::cout << "Exposure map: " << exp_map->Area() << " sq. degrees. (" <<
      pixelized_raw_area << " sq. degrees raw).\n" <<
      "Writing out to " << FLAGS_output_file << "...\n";
    exp_map->Write(FLAGS_output_file);
    std::cout << "Done.\n";
  } else {
    std::cout << "No pixels in map.  Exiting.\n";
  }

  return 0;
}
