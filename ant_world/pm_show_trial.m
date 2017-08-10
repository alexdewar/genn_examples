clear

%% load route file
fn = 'ant1_route1.bin';

d = dir(fn);
len = d(1).bytes;
route = NaN(len / (3*8), 3);

fid = fopen(fn,'r');
for i = 1:3
    route(:,i) = fread(fid,size(route,1),'double');
end
fclose(fid);
route(:,1) = route(:,1) / 100;
route(:,2) = route(:,2) / 100;
route(:,3) = 90 - route(:,3);

figure(1);clf
plot(route(:,1),route(:,2))
axis equal

%% load simulation data
recap = csvread('test.csv');

hold on
plot(recap(:,1),recap(:,2),'g--+',recap(1,1),recap(1,2),'ro')
anglequiver(recap(:,1),recap(:,2),recap(:,3),1,'g')