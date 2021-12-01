#include "Router.h"
#include "HttpServerTask.h"

using namespace wfrest;

void Router::handle(const char *route, const Handler &handler, const SeriesHandler &series_handler, Verb verb)
{
    auto &vh = routes_map_[route];
    vh.verb = verb;
    vh.path = route;
    vh.handler = handler;
    vh.series_handler = series_handler;
    vh.compute_queue_id = -1;
}

void Router::handle(const char *route, int compute_queue_id, const Handler &handler,
                    const SeriesHandler &series_handler, Verb verb)
{
    auto &vh = routes_map_[route];
    vh.verb = verb;
    vh.path = route;
    vh.handler = handler;
    vh.series_handler = series_handler;
    vh.compute_queue_id = compute_queue_id;
}

void Router::call(const std::string &verb, const std::string &route, HttpReq *req, HttpResp *resp) const
{
    // skip the last / of the url.
    // /hello ==  /hello/
    fprintf(stderr, "route : %s\n", route.c_str());
    StringPiece route2(route);
    if (!route2.empty() and route2[static_cast<int>(route2.size()) - 1] == '/')
        route2.remove_suffix(1);

    RouteParams route_params;
    auto it = routes_map_.find(route2, route_params);

    if (it != routes_map_.end())   // has route
    {
        auto *server_task = req->get_task();
        // match verb
        // it == <StringPiece : path, VerbHandler>
        if (it->second.verb == Verb::ANY or parse_verb(verb) == it->second.verb)
        {
            req->set_full_path(it->second.path);
            req->set_route_params(std::move(route_params));
            if (it->second.compute_queue_id == -1)
            {
                if (it->second.handler)
                    it->second.handler(req, resp);
                else
                    it->second.series_handler(req, resp, series_of(server_task));
            } else   // computing task
            {
                WFGoTask *go_task = WFTaskFactory::create_go_task(
                        "wfrest" + std::to_string(it->second.compute_queue_id),
                        it->second.handler,
                        req, resp);
                **server_task << go_task;
            }
        } else
        {
            resp->set_status(HttpStatusNotFound);
            fprintf(stderr, "verb %s not implemented on route %s\n", verb.c_str(), route2.data());
        }
    } else
    {
        resp->set_status(HttpStatusNotFound);
        fprintf(stderr, "Route dose not exist\n");
    }
}

Verb Router::parse_verb(const std::string &verb)
{
    if (strcasecmp(verb.c_str(), "GET") == 0)
        return Verb::GET;
    if (strcasecmp(verb.c_str(), "PUT") == 0)
        return Verb::PUT;
    if (strcasecmp(verb.c_str(), "POST") == 0)
        return Verb::POST;
    if (strcasecmp(verb.c_str(), "HTTP_DELETE") == 0)
        return Verb::HTTP_DELETE;
    return Verb::ANY;
}

void Router::print_routes()
{
    routes_map_.all_routes([](const std::string &prefix, const VerbHandler &h)
                           {
                               fprintf(stderr, "%s\n", prefix.c_str());
                           });
//    routes_map_.all_routes([](auto r, auto h) { std::cout << r << '\n'; });
}

//void Router::all_routes()
//{
//
//}
