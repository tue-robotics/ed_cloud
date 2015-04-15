#include "hypertable_reader.h"
#include <map>
#include <ros/ros.h>
#include <ed/world_model.h>
#include <ed/update_request.h>
#include <geolib/datatypes.h>
#include "world_writer.h"
#include "world_reader.h"
#include <ed/io/json_writer.h>
#include <ed/io/json_reader.h>
#include <geolib/Shape.h>
#include <geolib/Mesh.h>
#include <fstream>

HypertableReaderPlugin::HypertableReaderPlugin()
{
}

HypertableReaderPlugin::~HypertableReaderPlugin() {
    ROS_INFO("Writing file");
    std::ofstream ofile("output-client-0.json");
    ofile << world_text;
    ofile.close();
}

void HypertableReaderPlugin::initialize(ed::InitData& init) {
    total_elements = 0;
    last_update_time = 0;

    init.config.value("address", db_address);
    init.config.value("port", db_port);
    init.config.value("namespace", db_namespace);

    try {
        client = new Hypertable::Thrift::Client(db_address, db_port);
    }
    catch (Hypertable::ThriftGen::ClientException &e) {
      init.config.addError(e.message);
      ROS_ERROR_STREAM(e.message);
    }

}

void HypertableReaderPlugin::process(const ed::PluginInput& data, ed::UpdateRequest& req) {

    std::vector<Hypertable::ThriftGen::CellAsArray> cells_as_arrays;

    std::stringstream ofile;
    ed::io::JSONWriter writer(ofile);
    ed_cloud::world_write(data.world, 0, writer);
    world_text = ofile.str();


    try {
        Hypertable::ThriftGen::Namespace ns = client->namespace_open(db_namespace);

        std::stringstream query;

        query << "SELECT * FROM " << table_name
              << " WHERE TIMESTAMP > "
              << "\"" << get_last_timestamp_string() << "\""
              << " MAX_VERSIONS = 1";

        Hypertable::ThriftGen::HqlResultAsArrays result_as_arrays;

        client->hql_query_as_arrays(result_as_arrays, ns, query.str());

        if (!result_as_arrays.cells.empty()) {
            total_elements+=result_as_arrays.cells.size();
            ROS_INFO_STREAM("New data! " << result_as_arrays.cells.size()
                            << " Elements, Total " << total_elements);
            process_cells(result_as_arrays.cells, req);
        }

        req.setSyncUpdate(true);

        client->namespace_close(ns);
    } catch (Hypertable::ThriftGen::ClientException &e) {
        ROS_ERROR_STREAM(e.message);
    }
}

std::string HypertableReaderPlugin::get_last_timestamp_string()
{
    int64_t  date_msb, date_lsb;
    char timestamp[100];
    date_msb = last_update_time / 1e9;
    date_lsb = last_update_time % (long)1e9;

    struct tm *timestamp_tm = localtime(&date_msb);

    strftime (timestamp, 99, "%F %T",
                     timestamp_tm);
    sprintf(timestamp, "%s.%ld", timestamp, date_lsb);

    return std::string(timestamp);

}

void HypertableReaderPlugin::process_cells(std::vector<Hypertable::ThriftGen::CellAsArray> &cells, ed::UpdateRequest& req)
{
    ed::UUID current_entity;
    bool entity_removed = false;
    int64_t  timestamp;
    std::string publisher;
    for (auto & cell : cells) {

        timestamp = atol(cell[4].c_str());

        if (last_update_time < timestamp) {
            last_update_time = timestamp;
        }

        get_cell_publisher(cell, publisher);

        if (publisher == ros::this_node::getName() ||
                (current_entity == cell[0] && entity_removed)) {
            continue;
        } else {
            entity_removed = false;
            current_entity = cell[0];
            if (cell[1] == "deleted") {
                entity_removed = true;
                req.removeEntity(current_entity);
            } else {
                add_to_world_model(cell, req);
            }
        }
    }

}

void HypertableReaderPlugin::add_to_world_model(Hypertable::ThriftGen::CellAsArray &cell, ed::UpdateRequest &req)
{
    ed::UUID entity_id = cell[0];
    ed::io::JSONReader reader(cell[3].c_str());

    if (cell[1] == "pose") {
        geo::Pose3D pose;
        ed_cloud::read_pose(reader, pose);
        req.setPose(entity_id, pose);
    } else if (cell[1] == "shape") {
        geo::Mesh mesh;

        ed_cloud::read_shape(reader, mesh);
        geo::ShapePtr shape(new geo::Shape());

        shape->setMesh(mesh);
        req.setShape(entity_id, shape);

    }  else if (cell[1] == "convex_hull") {

        ed::ConvexHull2D ch;
        ed_cloud::read_convex_hull(reader, ch);

        req.setConvexHull(entity_id, ch);

    } else if (cell[1] == "type") {
        ed::TYPE type;
        ed_cloud::read_type(reader, type);
        req.setType(entity_id, type);
    }
}

void HypertableReaderPlugin::get_cell_publisher(Hypertable::ThriftGen::CellAsArray &cell, std::string &publisher)
{
    ed::io::JSONReader reader(cell[3].c_str());
    ed_cloud::read_publisher(reader, publisher);
}

ED_REGISTER_PLUGIN(HypertableReaderPlugin)
