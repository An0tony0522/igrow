/*

   Copyright (c) 2011, The Chinese University of Hong Kong

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

 */

/**
 * \mainpage igrow
 *
 * \section introduction Introduction
 * igrow is a multithreaded ligand growing tool for structure-based molecule design.
 *
 * \section features Features
 * igrow is inspired by AutoGrow. It uses idock as backend docking engine.
 * igrow supports more types of chemical synthesis such as halogen replacement and branch replacement in addition to hydrogen replacement.
 * igrow digests ligands and fragments in PDBQT format, saving the effort of frequently calling the prepare_ligand4 python script.
 * igrow invents its own thread pool in order to reuse threads and maintain a high CPU utilization throughout the entire synhsizing procedure. The thread pool parallelizes the creation of mutants and children in each generation.
 * igrow utilizes flyweight pattern for caching fragments and dynamic pointer vector for caching and sorting ligands.
 * igrow traces the sources of generated ligands and dumps the statistics in csv format so that users can easily get to know how the ligands are synthesized from the initial ligand and fragments.
 *
 * \section availability Availability
 * igrow is free and open source available at https://GitHub.com/HongjianLi/igrow under Apache License 2.0. Precompiled executables for 32-bit and 64-bit Linux, Windows, Mac OS X, FreeBSD and Solaris are provided.
 *
 * \author Hongjian Li, The Chinese University of Hong Kong.
 * \date 15 April 2012
 *
 * Copyright (C) 2011-2012 The Chinese University of Hong Kong.
 */

#include <boost/thread/thread.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/process/context.hpp>
#include <boost/process/operations.hpp>
#include <boost/process/child.hpp>
#include "seed.hpp"
#include "fstream.hpp"
#include "tee.hpp"
#include "ligand.hpp"
#include "thread_pool.hpp"
#include "operation.hpp"

int main(int argc, char* argv[])
{
	std::cout << "igrow 1.0\n";

	using namespace igrow;
	path initial_generation_csv_path, initial_generation_folder_path, fragment_folder_path, idock_config_path, output_folder_path, log_path, csv_path;
	size_t num_threads, seed, num_elitists, num_mutants, num_crossovers, max_failures, max_rotatable_bonds, max_atoms, max_heavy_atoms, max_hb_donors, max_hb_acceptors;
	fl max_mw, max_logp, min_logp;

	// Initialize the default path to log files. They will be reused when calling idock.
	const path default_log_path = "log.txt";
	const path default_csv_path = "log.csv";

	// Process program options.
	try
	{
		// Initialize the default values of optional arguments.
		const path default_output_folder_path = "output";
		const unsigned int concurrency = boost::thread::hardware_concurrency();
		const size_t default_num_threads = concurrency ? concurrency : 1;
		const size_t default_seed = random_seed();
		const size_t default_num_mutants = 20;
		const size_t default_num_crossovers = 20;
		const size_t default_num_elitists = 10;
		const size_t default_max_failures = 1000;
		const size_t default_max_rotatable_bonds = 30;
		const size_t default_max_atoms = 100;
		const size_t default_max_heavy_atoms = 80;
		const size_t default_max_hb_donors = 5;
		const size_t default_max_hb_acceptors = 10;
		const fl default_max_mw = 500;
		const fl default_max_logp = 5;
		const fl default_min_logp = -5;

		using namespace boost::program_options;
		options_description input_options("input (required)");
		input_options.add_options()
			("initial_generation_csv", value<path>(&initial_generation_csv_path)->required(), "path to initial generation csv")
			("initial_generation_folder", value<path>(&initial_generation_folder_path)->required(), "path to initial generation folder")
			("fragment_folder", value<path>(&fragment_folder_path)->required(), "path to folder of fragments in PDBQT format")
			("idock_config", value<path>(&idock_config_path)->required(), "path to idock configuration file")
			;

		options_description output_options("output (optional)");
		output_options.add_options()
			("output_folder", value<path>(&output_folder_path)->default_value(default_output_folder_path), "folder of output results")
			("log", value<path>(&log_path)->default_value(default_log_path), "log file in plain text")
			("csv", value<path>(&csv_path)->default_value(default_csv_path), "summary file in csv format")
			;

		options_description miscellaneous_options("options (optional)");
		miscellaneous_options.add_options()
			("threads", value<size_t>(&num_threads)->default_value(default_num_threads), "number of worker threads to use")
			("seed", value<size_t>(&seed)->default_value(default_seed), "explicit non-negative random seed")
			("elitists", value<size_t>(&num_elitists)->default_value(default_num_elitists), "number of elite ligands to carry over")
			("mutants", value<size_t>(&num_mutants)->default_value(default_num_mutants), "number of child ligands created by mutation")
			("crossovers", value<size_t>(&num_crossovers)->default_value(default_num_crossovers), "number of child ligands created by crossover")
			("max_failures", value<size_t>(&max_failures)->default_value(default_max_failures), "maximum number of operational failures to tolerate")
			("max_rotatable_bonds", value<size_t>(&max_rotatable_bonds)->default_value(default_max_rotatable_bonds), "maximum number of rotatable bonds")
			("max_atoms", value<size_t>(&max_atoms)->default_value(default_max_atoms), "maximum number of atoms")
			("max_heavy_atoms", value<size_t>(&max_heavy_atoms)->default_value(default_max_heavy_atoms), "maximum number of heavy atoms")
			("max_hb_donors", value<size_t>(&max_hb_donors)->default_value(default_max_hb_donors), "maximum number of hydrogen bond donors")
			("max_hb_acceptors", value<size_t>(&max_hb_acceptors)->default_value(default_max_hb_acceptors), "maximum number of hydrogen bond acceptors")
			("max_mw", value<fl>(&max_mw)->default_value(default_max_mw), "maximum molecular weight")
			("max_logp", value<fl>(&max_logp)->default_value(default_max_logp), "maximum logP")
			("min_logp", value<fl>(&min_logp)->default_value(default_min_logp), "minimum logP")
			("config", value<path>(), "options can be loaded from a configuration file")
			;

		options_description all_options;
		all_options.add(input_options).add(output_options).add(miscellaneous_options);

		// If no command line argument is supplied, simply print the usage and exit.
		if (argc == 1)
		{
			std::cout << all_options;
			return 0;
		}

		// Parse command line arguments.
		variables_map vm;
		store(parse_command_line(argc, argv, all_options), vm);
		variable_value config_value = vm["config"];
		if (!config_value.empty()) // If a configuration file is presented, parse it.
		{
			ifstream config_file(config_value.as<path>());
			store(parse_config_file(config_file, all_options), vm);
		}
		vm.notify(); // Notify the user if there are any parsing errors.

		using namespace boost::filesystem;

		// Validate initial generation csv.
		if (!exists(initial_generation_csv_path))
		{
			std::cerr << "Initial generation csv " << initial_generation_csv_path << " does not exist\n";
			return 1;
		}
		if (!is_regular_file(initial_generation_csv_path))
		{
			std::cerr << "Initial generation csv " << initial_generation_csv_path << " is not a regular file\n";
			return 1;
		}

		// Validate initial generation folder.
		if (!exists(initial_generation_folder_path))
		{
			std::cerr << "Initial generation folder " << initial_generation_folder_path << " does not exist\n";
			return 1;
		}
		if (!is_directory(initial_generation_folder_path))
		{
			std::cerr << "Initial generation folder " << initial_generation_folder_path << " is not a directory\n";
			return 1;
		}

		// Validate fragment folder.
		if (!exists(fragment_folder_path))
		{
			std::cerr << "Fragment folder " << fragment_folder_path << " does not exist\n";
			return 1;
		}
		if (!is_directory(fragment_folder_path))
		{
			std::cerr << "Fragment folder " << fragment_folder_path << " is not a directory\n";
			return 1;
		}

		// Validate idock configuration file.
		if (!exists(idock_config_path))
		{
			std::cerr << "idock configuration file " << idock_config_path << " does not exist\n";
			return 1;
		}
		if (!is_regular_file(idock_config_path))
		{
			std::cerr << "idock configuration file " << idock_config_path << " is not a regular file\n";
			return 1;
		}

		// Validate output folder.
		remove_all(output_folder_path);
		if (!create_directories(output_folder_path))
		{
			std::cerr << "Failed to create output folder " << output_folder_path << '\n';
			return 1;
		}

		// Validate log_path.
		if (is_directory(log_path))
		{
			std::cerr << "Log path " << log_path << " is a directory\n";
			return 1;
		}

		// Validate csv_path.
		if (is_directory(csv_path))
		{
			std::cerr << "csv path " << csv_path << " is a directory\n";
			return 1;
		}

		// Validate miscellaneous options.
		if (!num_threads)
		{
			std::cerr << "Option threads must be 1 or greater\n";
			return 1;
		}
		if (max_mw <= 0)
		{
			std::cerr << "Option max_mw must be positive\n";
			return 1;
		}
		if (min_logp > max_logp)
		{
			std::cerr << "Option max_mw must be larger than or equal to option min_mw\n";
			return 1;
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << '\n';
		return 1;
	}

	try
	{
		// Initialize the log.
		igrow::tee log(log_path);
		std::cout << "Logging to " << log_path << '\n';

		// The number of ligands (i.e. population size) is equal to the number of elitists plus mutants plus children.
		const size_t num_children = num_mutants + num_crossovers;
		const size_t num_ligands = num_elitists + num_children;
		const fl num_elitists_inv = static_cast<fl>(1) / num_elitists;

		// Initialize a pointer vector to dynamically hold and destroy generated ligands.
		boost::ptr_vector<ligand> ligands;
		ligands.resize(num_ligands);

		// Parse the initial generation csv to get initial elite ligands.
		{
			ifstream in(initial_generation_csv_path);
			string line;
			line.reserve(80);
			getline(in, line); // Ligand,Conf,FE1,FE2,FE3,FE4,FE5,FE6,FE7,FE8,FE9
			for (size_t i = 0; i < num_elitists; ++i)
			{
				// Check if there are sufficient initial elite ligands.
				if (!getline(in, line))
				{
					std::cerr << "Failed to construct initial generation because the initial generation csv " << initial_generation_csv_path << " contains less than " << num_elitists << " ligands.\n";
					return 1;
				}

				// Parse the elite ligand.
				const size_t comma1 = line.find_first_of(',', 1);
				ligands.replace(i, new ligand(initial_generation_folder_path / (line.substr(0, comma1) + ".pdbqt")));

				// Parse the free energy.
				const size_t comma2 = line.find_first_of(',', comma1 + 2);
				const size_t comma3 = line.find_first_of(',', comma2 + 6);
				ligands[i].free_energy = right_cast<fl>(line, comma2 + 2, comma3); // right_cast is 1-based.
			}
		}

		// Scan the fragment folder to obtain a list of fragments.
		log << "Scanning fragment folder " << fragment_folder_path << '\n';
		vector<path> fragments;
		fragments.reserve(1000); // A fragment folder typically consists of <= 1000 fragments.
		{
			using namespace boost::filesystem;
			const directory_iterator end_dir_iter; // A default constructed directory_iterator acts as the end iterator.
			for (directory_iterator dir_iter(fragment_folder_path); dir_iter != end_dir_iter; ++dir_iter)
			{
				// Skip non-regular files such as folders.
				if (!is_regular_file(dir_iter->status())) continue;

				// Save the fragment path.
				fragments.push_back(dir_iter->path());
			}
		}
		log << "Found " << fragments.size() << " fragments\n";

		// Initialize a Mersenne Twister random number generator.
		log << "Using random seed " << seed << '\n';
		mt19937eng eng(seed);

		// Initialize a ligand validator.
		const validator v(max_rotatable_bonds, max_atoms, max_heavy_atoms, max_hb_donors, max_hb_acceptors, max_mw, max_logp, min_logp);

		// Initialize the number of failures. The program will stop if num_failures reaches max_failures.
		boost::atomic<size_t> num_failures(0);

		// Reserve storage for operation tasks.
		operation op(ligands, num_elitists, fragments, v, max_failures, num_failures);
		boost::ptr_vector<packaged_task<void>> operation_tasks(num_children);

		// Initialize ligand filenames.
		vector<string> ligand_filenames;
		ligand_filenames.reserve(num_ligands);
		for (size_t i = 1; i <= num_ligands; ++i)
		{
			ligand_filenames.push_back(lexical_cast<string>(i) + ".pdbqt");
		}

		// Find the full path to idock executable.
		const path idock_path = path(boost::process::find_executable_in_path("idock")).make_preferred();
		log << "Using idock executable at " << idock_path << '\n';

		// Initialize argument to idock.
		vector<string> idock_args(12);
		idock_args[0]  = "--ligand_folder";
		idock_args[2]  = "--output_folder";
		idock_args[4]  = "--log";
		idock_args[6]  = "--csv";
		idock_args[8]  = "--seed";
		idock_args[9]  = lexical_cast<string>(seed);
		idock_args[10] = "--config";
		idock_args[11] = idock_config_path.string();

		// Initialize process context.
		const boost::process::context ctx;

		// Initialize a thread pool and create worker threads for later use.
		log << "Creating a thread pool of " << num_threads << " worker thread" << ((num_threads == 1) ? "" : "s") << '\n';
		thread_pool tp(num_threads);

		// Initialize csv file for dumping statistics.
		ofstream csv(csv_path);
		csv << "generation,ligand,parent 1,connector 1,parent 2,connector 2,free energy in kcal/mol,no. of rotatable bonds,no. of atoms,no. of heavy atoms,no. of hydrogen bond donors,no. of hydrogen bond acceptors,molecular weight,logP\n";

		for (size_t generation = 1; true; ++generation)
		{
			log << "Running generation " << generation << '\n';

			// Initialize the paths to current generation folder and its two subfolders.
			const path generation_folder(output_folder_path / lexical_cast<string>(generation));
			const path ligand_folder(generation_folder / "ligand");
			const path output_folder(generation_folder / "output");

			// Create a new folder and two subfolders for current generation.
			create_directory(generation_folder);
			create_directory(ligand_folder);
			create_directory(output_folder);

			// Create mutation and crossover tasks.
			BOOST_ASSERT(operation_tasks.empty());
			for (size_t i = 0; i < num_mutants; ++i)
			{
				operation_tasks.push_back(new packaged_task<void>(boost::bind<void>(&operation::mutation_task, boost::ref(op), num_elitists + i, ligand_folder / ligand_filenames[i], eng())));
			}
			for (size_t i = num_mutants; i < num_children; ++i)
			{
				operation_tasks.push_back(new packaged_task<void>(boost::bind<void>(&operation::crossover_task, boost::ref(op), num_elitists + i, ligand_folder / ligand_filenames[i], eng())));
			}

			// Run the mutation and crossover tasks in parallel asynchronously.
			tp.run(operation_tasks);

			// Propagate possible exceptions thrown by the mutation and crossover tasks.
			for (size_t i = 0; i < num_children; ++i)
			{
				operation_tasks[i].get_future().get();
			}

			// Block until all the mutation and crossover tasks are completed.
			tp.sync();
			operation_tasks.clear();

			// Invoke idock.
			idock_args[1] = ligand_folder.string();
			idock_args[3] = output_folder.string();
			idock_args[5] = (generation_folder / default_log_path).string();
			idock_args[7] = (generation_folder / default_csv_path).string();
			const int exit_code = create_child(idock_path.string(), idock_args, ctx).wait();
			if (exit_code)
			{
				log << "idock exited with code " << exit_code << '\n';
				return 1;
			}

			// Parse docked ligands to obtain predicted free energy and docked coordinates, and save the updated ligands into the ligand subfolder.
			for (size_t i = 0; i < num_children; ++i)
			{
				ligands[num_elitists + i].update(output_folder / ligand_filenames[i]);
			}

			// Sort ligands in ascending order of efficacy.
			ligands.sort();

			// Write summaries to csv and calculate average statistics.
			for (size_t i = 0; i < num_ligands; ++i)
			{
				const ligand& l = ligands[i];
				csv << generation
					<< ',' << l.p
					<< ',' << l.parent1
					<< ',' << l.c1srn
					<< ',' << l.parent2
					<< ',' << l.c2srn
					<< ',' << l.free_energy
					<< ',' << l.num_rotatable_bonds
					<< ',' << l.num_atoms
					<< ',' << l.num_heavy_atoms
					<< ',' << l.num_hb_donors
					<< ',' << l.num_hb_acceptors
					<< ',' << l.mw
					<< ',' << l.logp
					<< '\n';
			}

			// Calculate average statistics of elite ligands.
			fl avg_mw = 0, avg_logp = 0, avg_free_energy = 0, avg_rotatable_bonds = 0, avg_atoms = 0, avg_heavy_atoms = 0, avg_hb_donors = 0, avg_hb_acceptors = 0;
			for (size_t i = 0; i < num_elitists; ++i)
			{
				const ligand& l = ligands[i];
				avg_mw += l.mw;
				avg_logp += l.logp;
				avg_free_energy += l.free_energy;
				avg_rotatable_bonds += l.num_rotatable_bonds;
				avg_atoms += l.num_atoms;
				avg_heavy_atoms += l.num_heavy_atoms;
				avg_hb_donors += l.num_hb_donors;
				avg_hb_acceptors += l.num_hb_acceptors;
			}
			avg_mw *= num_elitists_inv;
			avg_logp *= num_elitists_inv;
			avg_free_energy *= num_elitists_inv;
			avg_rotatable_bonds *= num_elitists_inv;
			avg_atoms *= num_elitists_inv;
			avg_heavy_atoms *= num_elitists_inv;
			avg_hb_donors *= num_elitists_inv;
			avg_hb_acceptors *= num_elitists_inv;
			log << "Failures |  Avg FE |   Avg A |  Avg HA | Avg MWT | Avg NRB | Avg HBD | Avg HBA | Avg LogP\n"
			    << std::setw(8) << num_failures << "   "
				<< std::setw(7) << avg_free_energy << "   "
				<< std::setw(7) << avg_atoms << "   "
				<< std::setw(7) << avg_heavy_atoms << "   "
				<< std::setw(7) << avg_mw << "   "
				<< std::setw(7) << avg_rotatable_bonds << "   "
				<< std::setw(7) << avg_hb_donors << "   "
				<< std::setw(7) << avg_hb_acceptors << "   "
				<< std::setw(8) << avg_logp << '\n';
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << '\n';
		return 1;
	}
}
