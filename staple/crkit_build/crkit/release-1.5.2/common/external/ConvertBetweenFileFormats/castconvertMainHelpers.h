/*=========================================================================

  Program:   Insight Segmentation & Registration Toolkit
  Module:    $RCSfile: castconverthelpers2.h,v $
  Language:  C++
  Date:      $Date: 2006/06/09 09:29:43 $
  Version:   $Revision: 1.1 $

  Copyright (c) 2002 Insight Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef __castconvertMainHelpers_h__
#define __castconvertMainHelpers_h__

#include "itkCommandLineArgumentParser.h"
#include <itksys/SystemTools.hxx>
#include "itkGDCMSeriesFileNames.h"

/**
 * ******************* PrintHelp *******************
 */

void PrintHelp()
{
	std::cout << "Usage:" << std::endl << "ConvertBetweenFileFormats " << std::endl;
	std::cout << "\t-in\tinputfilename" << std::endl;
	std::cout << "\t-out\toutputfilename" << std::endl;
	std::cout << "\t[-opct]\toutputPixelComponentType" << std::endl;
	std::cout << "OR pxcastconvert" << std::endl;
	std::cout << "\t-in\tdicomDirectory" << std::endl;
	std::cout << "\t-out\toutputfilename" << std::endl;
	std::cout << "\t[-opct]\toutputPixelComponentType" << std::endl;
	std::cout << "\t[-s]\tseriesUID" << std::endl;
	std::cout << "where outputPixelComponentType is one of:" << std::endl;
	std::cout << "\tunsigned_char, char, unsigned_short, short, unsigned_int, int," << std::endl;
	std::cout << "\tunsigned_long, long, float, double," << std::endl;
	std::cout << "provided that the outputPixelComponentType is supported by the output file format." << std::endl;
	std::cout << "By default the outputPixelComponentType is set to the inputPixelComponentType." << std::endl;
	std::cout << "By default the seriesUID is the first UID found." << std::endl;

} // end PrintHelp


/**
 * ******************* GetCommandLineArguments *******************
 */

int GetCommandLineArguments( int argc, char **argv, std::string & errorMessage,
	std::string & input, std::string & outputFileName,
	std::string & outputPixelComponentType, std::string & seriesUID )
{
	/** Create a command line argument parser. */
	itk::CommandLineArgumentParser::Pointer parser = itk::CommandLineArgumentParser::New();
	parser->SetCommandLineArguments( argc, argv );

	/** Get arguments. */
	bool retin = parser->GetCommandLineArgument( "-in", input );
	bool retout = parser->GetCommandLineArgument( "-out", outputFileName );
	bool retopct = parser->GetCommandLineArgument( "-opct", outputPixelComponentType );
        if (!retopct) {
          ; // No need to specify an opct if you don't want to.
        }

	bool rets = parser->GetCommandLineArgument( "-s", seriesUID );
        if (!rets) {
          ; // No need to specify a seriesUID if you don't want to.
        }

	/** Check if necessary command line arguments are available. */
	if ( !retin )
	{
		errorMessage = "ERROR: You should specify \"-in\".";
		return 1;
	}
	if ( !retout )
	{
		errorMessage = "ERROR: You should specify \"-out\".";
		return 1;
	}

	/** Check outputPixelType. */
  if ( outputPixelComponentType != ""
    && outputPixelComponentType != "unsigned_char"
    && outputPixelComponentType != "char"
    && outputPixelComponentType != "unsigned_short"
    && outputPixelComponentType != "short"
    && outputPixelComponentType != "unsigned_int"
    && outputPixelComponentType != "int"
    && outputPixelComponentType != "unsigned_long"
    && outputPixelComponentType != "long"
    && outputPixelComponentType != "float"
    && outputPixelComponentType != "double" )
  {
    /** In this case an illegal outputPixelComponentType is given. */
    errorMessage = "The given outputPixelComponentType is \""
			+ outputPixelComponentType + "\", which is not supported.";
    return 1;
  }

	/** Return a value. */
	return 0;

} // end GetCommandLineArguments


/**
 * ******************* IsDICOM *******************
 */

int IsDICOM( std::string & input, std::string & errorMessage, bool & isDICOM )
{
  /** Make sure last character of input != "/".
   * Otherwise FileIsDirectory() won't work.
	 */
  if ( input.rfind( "/" ) == input.size() - 1 )
  {
    input.erase( input.size() - 1, 1 );
  }

  /** Check if input is a file or a directory. */
  bool exists = itksys::SystemTools::FileExists( input.c_str() );
  bool isDir = itksys::SystemTools::FileIsDirectory( input.c_str() );

  if ( exists && !isDir ) isDICOM = false;
  else if ( exists && isDir ) isDICOM = true;
  else
  {
    /** Something is wrong. */
    errorMessage = "ERROR: " + input + " does not exist.";
    return 1;
  }

	/** Return a value. */
	return 0;

} // end IsDICOM


/**
 * ******************* GetFileNameFromDICOMDirectory *******************
 */

int GetFileNameFromDICOMDirectory( std::string & seriesUID, std::string & inputDirectoryName,
	std::string & errorMessage, std::string & fileName )
{
	typedef itk::GDCMSeriesFileNames                GDCMNamesGeneratorType;
  typedef std::vector< std::string >              FileNamesContainerType;

	/** Create vector of filenames from the DICOM directory. */
	GDCMNamesGeneratorType::Pointer nameGenerator = GDCMNamesGeneratorType::New();
	nameGenerator->SetUseSeriesDetails( true );
	nameGenerator->SetInputDirectory( inputDirectoryName.c_str() );

	/** The short and fast way. */
	FileNamesContainerType fileNames;
	if ( seriesUID == "" )
	{
		fileNames = nameGenerator->GetInputFileNames();
		fileName = fileNames[ 0 ];
		return 0;
	}

	/** Get all series in this directory. */
	FileNamesContainerType seriesNames = nameGenerator->GetSeriesUIDs();
	if ( !seriesNames.size() )
	{
		errorMessage = "ERROR: no DICOM series in directory " + inputDirectoryName + ".";
		return 1;
	}

	/** Get a list of files in series. */
	fileNames = nameGenerator->GetFileNames( seriesUID );
	if ( !fileNames.size() )
	{
		errorMessage = "ERROR: no DICOM series " + seriesUID
			+ " in directory " + inputDirectoryName + ".";
		return 1;
	}
	
	/** Get a name of a 2D image. */
	fileName = fileNames[ 0 ];

	/** Return a value. */
	return 0;

} // end GetFileNameFromDICOMDirectory


#endif //__castconvertMainHelpers_h__
