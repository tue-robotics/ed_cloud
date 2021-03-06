#include "sync_client_plugin.h"

#include <ed/update_request.h>
#include <ed/entity.h>
#include <ed/world_model.h>
#include <geolib/ros/msg_conversions.h>
#include <geolib/Mesh.h>
#include <geolib/Shape.h>

#include "ed_cloud/EntityUpdateInfo.h"
#include "ed_cloud/GetWorldModel.h"
#include "ed_cloud/Polygon.h"
#include "ed_cloud/Mesh.h"
#include "world_writer.h"

#include <ros/node_handle.h>
#include <fstream>
#include <sstream>
#include <ed/io/json_writer.h>

// ----------------------------------------------------------------------------------------------------

SyncClient::SyncClient()
{
    current_rev_number = 0;
}

// ----------------------------------------------------------------------------------------------------

SyncClient::~SyncClient()
{
}

// ----------------------------------------------------------------------------------------------------

void SyncClient::configure(tue::Configuration config)
{
}

// ----------------------------------------------------------------------------------------------------

void SyncClient::initialize()
{
    current_rev_number = 0;

    ros::NodeHandle n;
    client = n.serviceClient<ed_cloud::GetWorldModel>("/ed/get_world");
}

// ----------------------------------------------------------------------------------------------------

void SyncClient::process(const ed::WorldModel &world, ed::UpdateRequest &req)
{
//    std::stringstream fileName;
//    ROS_INFO("Writing file");
//    fileName << "output-client-";
//    fileName << this->current_rev_number;
//    fileName << ".json";
//    std::ofstream ofile(fileName.str().c_str());
//    ed::io::JSONWriter writer(ofile);
//    ed_cloud::world_write(world, this->current_rev_number, writer);
//    ofile.close();

    ed_cloud::GetWorldModel srv;

    srv.request.rev_number = this->current_rev_number;

    if (client.call(srv)) {
        updateWithDelta(srv.response.world, world, req);
        this->current_rev_number = srv.response.rev_number;
    } else {
        ROS_WARN("Cannot obtain world model updates from the server");
    }
}

// ----------------------------------------------------------------------------------------------------

void SyncClient::updateWithDelta(ed_cloud::WorldModelDelta& a,
                                        const ed::WorldModel &world,
                                        ed::UpdateRequest &req)
{
    for (std::vector<ed_cloud::EntityUpdateInfo>::const_iterator it = a.update_entities.begin();
        it != a.update_entities.end(); it++) {

        if (it->id.empty()) {
            if (it->index < entity_index.size())
            {
                req.removeEntity(entity_index[it->index]);
                std::map<ed::UUID, long>::iterator pos = index_map.find(entity_index[it->index]);
                if (pos != index_map.end()) {
                    index_map.erase(pos);
                    entity_index[it->index] = std::string();
                }
            }
        } else {
            if (it->index < entity_index.size())
            {
                ed::UUID old_id = entity_index[it->index];
                if (!old_id.str().empty() && old_id != it->id)
                {
                    req.removeEntity(entity_index[it->index]);
                }
            } else {
                entity_index.resize(it->index + 1, std::string());
            }

            entity_index[it->index] = it->id;
            index_map[it->id] = it->index;

            req.setType(it->id, it->type);

            if (it->new_pose) {
                geo::Pose3D pose;
                geo::convert(it->pose, pose);
                    req.setPose(it->id, pose);
            }

            if (it->new_shape_or_convex) {
                if (it->is_convex_hull) {
                    ed::ConvexHull2D ch;
                    geo::Pose3D center;
                    geo::convert(it->center, center);
                    ch.center_point = center.t;
                    ch.max_z = it->polygon.z_max;
                    ch.min_z = it->polygon.z_min;
                    for (int i = 0; i < it->polygon.xs.size(); i ++) {
                        pcl::PointXYZ p (it->polygon.xs[i], it->polygon.ys[i], 0);
                        ch.chull.points.push_back(p);
                    }

                    req.setConvexHull(it->id, ch);
                } else {
                    geo::Mesh m;

                    for (int i = 0; i < it->mesh.vertices.size(); i += 3) {

                        m.addPoint(it->mesh.vertices[i],
                                   it->mesh.vertices[i + 1],
                                   it->mesh.vertices[i + 2]);
                    }

                    for (int i = 0; i < it->mesh.triangles.size(); i += 3) {
                        m.addTriangle(it->mesh.triangles[i],
                                      it->mesh.triangles[i + 1],
                                      it->mesh.triangles[i + 2]);
                    }

                    geo::ShapePtr shape(new geo::Shape());
                    shape->setMesh(m);
                    req.setShape(it->id, shape);
                }
            }
        }
    }

    for (std::vector<std::string>::const_iterator it = a.remove_entities.begin();
         it != a.remove_entities.end(); it++) {

            std::map<ed::UUID, long>::iterator pos = index_map.find(*it);
            if (pos != index_map.end()) {
                req.removeEntity(*it);
                index_map.erase(pos);
                entity_index[index_map[*it]] = std::string();
            }
    }
}

ED_REGISTER_PLUGIN(SyncClient)
