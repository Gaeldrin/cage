
set(cage_output_data_source "${CMAKE_CURRENT_LIST_DIR}")

function(cage_output_data where)
	if(NOT EXISTS "${where}/data")
		file(MAKE_DIRECTORY "${where}/data")
	endif()
	cage_directory_link("${cage_output_data_source}/../data" "${where}/data/cage")
	configure_file("${cage_output_data_source}/../include/cage-client/graphics/shaderConventions.h" "${where}/data" COPYONLY)
	set(input "${where}/data")
	set(output "${where}/assets")
	set(schemes "${cage_output_data_source}/../schemes")
	set(database "${where}")
	foreach(conf IN ITEMS ${CMAKE_CONFIGURATION_TYPES} ${CMAKE_BUILD_TYPE})
		string(TOUPPER ${conf} conf_upper)
		configure_file("${cage_output_data_source}/cage-asset-database.in.ini" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_${conf_upper}}/cage-asset-database.ini")
	endforeach(conf)
endfunction(cage_output_data)

